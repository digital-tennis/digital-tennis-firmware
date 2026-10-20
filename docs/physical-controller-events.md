# DT-SDD-31-30-00 · 물리 컨트롤러 이벤트와 게임 해석

## 00 문서 관리

R01 / 2026-10-20 / 구현 검토본. NU40-DK+별도 외장 IMU가 제품이며 NU-54는 센서 없는 임시 버튼 시험 타깃이다. 원래12일·3명·36PD 계획을 보존한다. DT 번호는 자체 관리 형식이며 ATA iSpec2200 공식 적합성을 주장하지 않는다.

## 10 목적과 책임

마우스가 버튼Down/Up을 보내고 상위 소프트웨어가 클릭/드래그를 해석하듯, 보드는 **측정한 움직임과 버튼 번호**를 보낸다. 공의 상태를 모르는 보드에 서브·스매시를 고르게 하지 않는다.

```text
external_imu.c → SI6축·단조ms → dt_controller_tick
  → 정지 보정·장착 축·필터·구간 → dt_gesture_classify_ring
  → v3 MOTION / 번호별 BUTTON_DOWN·BUTTON_UP
  → BLE → SDK(연결/보정/중복/시각 검증)
  → game SensorPlayerInputSource → 공통 adapter
  → 온라인이면 호스트 PhysicalTennisInputResolver
  → 기존 SubmitInput → 거리·높이·타이밍·중복·실제 물리/점수
```

| 저장소 | 담당 | 게임 문맥 |
| --- | --- | --- |
| firmware | 외장 IMU 취득/보정/축·6종 물리 분류, 버튼edge, 송신·보안 | 갖지 않음 |
| protocol | v3 wire·단위·CRC·버전·벡터 | 갖지 않음 |
| ble-unity | OS 연결, 보정 ACK, 현재 세대/중복/수명, 중립 DTO | 갖지 않음 |
| match-server | 중립 문자열 허용·방/역할/epoch 검증·중계 | 해석하지 않음 |
| game | 버튼 바인딩, 물리→테니스 해석, 실제 타격 규칙 | 호스트가 소유 |

raw6축은 일반 BLE 이벤트로 스트리밍하지 않는다.20바이트는 기존 MTU23 호환 설계이며 BLE 전체 처리량의 절대 한도가 아니다. 원시 기록은 개발용 UART→CSV로 분리한다.

## 20 wire와 기본 모드

| v3 type | 값 | kind |
| --- | --- | --- |
| MOTION | 4 | 1횡회전+,2횡회전−,3위→아래,4짧은 회전,5대각 하강,6대각 상승 |
| BUTTON_DOWN | 5 | 물리 버튼 번호1..5 |
| BUTTON_UP | 6 | 같은 번호. 세기·각도는0 |

패킷은20바이트이며 새 입력의 eventId는0이 아니다. 재전송은 같은 eventId/장치 시각을 유지하고 transport sequence만 새로 쓴다. 버튼은30ms 디바운스 후 변화마다 한 번씩 생성하고, 부팅 때 눌린 버튼은 떼었다가 다시 누르기 전까지 억제한다.

SDK는 기존 v2 LINK_PROBE를 보내고 새 펌웨어는 v3 LINK_REPLY.kind=2(PhysicalMotion) 또는3(PhysicalButtons)으로 응답한다. 모션 프로필은 현재 연결의 성공 CALIBRATE ACK와 ready/calibrated flags가 필요하다. 버튼 전용은 센서 준비를 위조하지 않고 입력한다. 구형 v1/v2의 샷/버튼 ID는 그대로이며 SDK가 별도로 해석한다. 구형 SDK는 새 v3를 거부한다. 전체 규격은 protocol의 `spec/controller-v3.md`가 기준이다.

`src/main.c`의 기본 앱은 `dt_motion_physical_defaults()`다. NU40은 PhysicalMotion, 센서 없는 NU54는 PhysicalButtons를 사용한다. 기본 필터alpha0.4·시작180/해제80deg/s와 수치 범위는 CSV에서 빌드 시 생성한다. 전체 구간 종료에서 판정하므로 피크 즉시 반응하는 구현이 아니다. 실물 튜닝 전 휴리스틱이며 스윙 시작/피크 실시간 스트림도 아니다.

## 30 수정할 곳

| 작업 | 파일/함수 | 바꿀 내용 |
| --- | --- | --- |
| 센서 모델/핀/단위 | `overlays/`, `src/hw/driver/external_imu.c` | SI 가속도m/s²·gyro deg/s·단조ms, read 실패는false |
| 장착·좌손·구간 | `src/ap/dt_controller.c`, `dt_motion_physical_defaults` | 축 행렬/보정/필터. 경기 phase를 추가하지 않음 |
| 물리 특징/범위 | `src/ap/dt_gesture.c`, `config/gesture-rules.csv`, `config/gesture-detector.csv` | 같은 장착 기준의6물리 클래스. CSV 변경 후 재빌드 |
| 버튼핀/edge | 보드overlay, `src/hw/driver/buttons.c`, `dt_button_edge_poll` | 번호·Down/Up. 스킬/서브 바인딩은 추가하지 않음 |
| 큐/만료 | `src/ap/dt_tx_queue.c` | 입력 TTL200ms와 재연결 purge 보존 |
| 새필드/새물리ID | protocol C/C#·명세·벡터 | 먼저 규격 변경. 기존v1/v2 덮어쓰기 금지 |
| DTO/상태 | SDK `BleControllerClient.TryRead` | 중립 kind/edge 보존. 테니스 상태 참조 금지 |
| 테니스 의미 | game `PhysicalTennisInputResolver.TryResolve` | 서브권·phase·bounce→액션. 기존 타격gate를 우회하지 않음 |

`dt_controller_set_context`와 경기 상태 enum은 제거했다. `set_mode/set_rules/intent/button` 레거시 API는 물리 프로필에서 거부한다. 비교용 v2 휴리스틱의 `serve_dps`, 샷 이름은 남아 있지만 새 보드 기본 경로는 사용하지 않는다.

## 40 재현 과제와 완료 조건

1. CMake host와 sanitizers 검사에서 v1/v2 유지·v3 물리6종·버튼Down/Up/boot-held/debounce·eventId wrap·TTL을 확인한다.
2. `simulator/server.py`에서 두 위→아래 궤적이 모두 kind3인지 확인한다. GUI/CSV/C API 어디에도 경기 상태를 전달하지 않는다. 장착/좌손·잡음·일상동작을 평가한다.
3. 같은 C 패킷을 protocol→SDK→Unity 입력 source에 넣는다. 설정에서는 물리 이름을 보이고 테스트 입력은 경기에 누출되지 않아야 한다.
4. Unity 실제 MatchCourt에서 같은kind3가 자기서브 때 Serve, 랠리의 합법적인 높은 공에서 Smash, 낮은 공에서 Height 거부인지 확인한다. 짧은 회전은 바운드 전 Volley, 바운드 후 일반 스트로크다. 타인서브·dead 상태에서 거부한다.
5. 온라인 게스트는 중립 입력을 보내고 호스트가 실제 공 상태로 해석한다. 게스트의 추정 phase로 샷을 미리 확정하지 않는다.

합성 궤적·mock BLE·배치 Unity 검사는 실제 손동작/무선 지연의 증거가 아니다. 후속 실물 담당자는 [인수 가이드](imu-developer-handoff.md)에 따라 배선·장착·사용자/세션 분리 데이터와 정밀도/재현율·오검출/분·지연분포를 기록한다. 이번 변경에서는 flash·물리 IMU·추가 NU54 버튼 검증을 하지 않는다.

## 50 이번 변경의 소프트웨어 증거

[검증 JSON](physical-events-validation.json)에 고정CSV 해시·시드·혼동행렬·실패표본·4보드 ELF 해시를 남겼다. 새 평가 시드741103의 합성700동작 중699개 인식,500일상동작 오검출0·중복0이었다. 횡회전−1개 누락이며 피크 이후 판정150ms 중앙값/330ms 최대다. 실물 정확도/무선 지연이 아니다. 임계값은 이 시드를 보고 다시 맞추지 않았다.
