# DT-SDD-31-10-00 · 외장 IMU 물리 컨트롤러 구조

R03 / 2026-10-20. 제품은 **NU40-DK + 별도 부착 외장 IMU**다. NU54는 센서 없는 BLE/버튼 시험 타깃이다. 최초12일·3명·36PD 계획을 보존한다. DT 번호는 자체 관리 형식이며 ATA iSpec2200 공식 적합성을 주장하지 않는다.

## 10 현재 기본 경로

```text
external_imu.c → m/s², deg/s, monotonic ms
 → dt_controller_tick → 정지 보정·장착 축·필터·구간
 → dt_gesture_classify_ring → 6종 중립 물리 동작
 → v3 MOTION / BUTTON_DOWN / BUTTON_UP
 → dt_tx_queue → 암호화된 BLE Notify → SDK
 → Unity 게임이 서브권·공 높이·바운드·타격 가능 거리를 보고 테니스 액션 판정
```

보드는 공·경기 phase·선수 권한을 알지 않는다. 같은 OverheadDown은 Unity에서 서브 또는 스매시 후보가 되며 기존 타격 gate를 통과해야 한다. `dt_controller_set_context`와 경기 상태 enum은 제거했다. 버튼 번호/물리 동작을 테니스 액션으로 바꾸는 작업은 게임 저장소에 둔다.

| 위치 | 책임 |
| --- | --- |
| `boards/`, `overlays/` | 검토한 GPIO·별도 센서 alias. 핀·모델을 추정하지 않음 |
| `src/hw/driver/external_imu.c` | Zephyr sample_fetch/accel/gyro, SI 단위 변환, 실패 보고 |
| `src/ap/dt_controller.c` | 보정·시간 검증·축·필터·구간, 명령 처리 |
| `src/ap/dt_gesture.c`, `config/gesture-*.csv` | 방향/회전/시간/일관성 특징과 물리6종 분류 |
| `src/ap/dt_button.c` | 30ms debounce, 번호별 Down/Up, boot-held 억제 |
| `src/ap/ble_peripheral.c` | Peripheral/GATT, 연결·암호화·본딩·구독 |
| `src/ap/dt_tx_queue.c` | 32개 큐·TTL·재전송·연결 세대 |
| `src/main.c` | 100Hz 취득, `dt_motion_physical_defaults()`, 독립 TX worker |
| protocol 고정 커밋 | 공통20바이트 코덱·CRC·명세·호환 벡터 |

## 20 센서·분류와 지연

별도 센서는 확인한 overlay의 `external-imu` alias로 연결한다. alias 없음/미준비/읽기 실패는 ready/calibrated=false이며 동작을 만들지 않는다. 가속도는m/s², gyro는deg/s, 시각은단조ms다. 장착 행렬·왼손 축 처리는 보정/필터보다 앞에서 일관되게 적용한다.

CALIBRATE는 정지64개 연속 샘플로 gyro bias를 구하고 움직이면 다시 모은다. 5초 실패·센서 실패·시간 불연속·끊김은 보정을 무효화한다. 완료된 명령ID의 ACK는 최근64개 결과를 재사용한다.

기본은 **전체 구간 종료 판정**이다. `dt_motion_physical_defaults()`의 필터alpha0.4, 시작180/해제80deg/s 등은 CSV에서 생성한 초기 합성 실험값이다. 피크 즉시 송신 방식이 아니다. raw6축은 일반 BLE 이벤트로 스트리밍하지 않고 개발용 UART→CSV로 기록한다.20바이트는 MTU23 호환 설계이며 BLE 전체 처리량 한도가 아니다.

물리 kind1..6은 횡회전+, 횡회전−, 위→아래, 짧은 회전, 대각 하강, 대각 상승이다. 구형 v2 샷 모드·피크 API는 비교/호환용이며 새 보드 기본 경로가 아니다. 물리 프로필은 레거시 `set_mode/set_rules/intent/button` API를 거부한다. 분류 규칙 CSV 변경은 펌웨어 재빌드가 필요하다. 게임의 스킬 패턴/밸런스 CSV와 혼동하지 않는다.

## 30 메시지와 버튼

v3 MOTION=4, BUTTON_DOWN=5, BUTTON_UP=6이다. 새 이벤트ID는0이 아니고 같은 이벤트 재전송은ID/원래deviceTimeMs를 유지하며 transport sequence만 새로 쓴다. Down/Up은30ms 안정 변화마다1회, 부팅 때 눌린 버튼은 완전히 뗀 뒤부터 유효하다. 번호1..5의 게임 의미는 보드에 넣지 않는다.

SDK의 v2 LINK_PROBE에 새 보드는 v3 LINK_REPLY.kind=2(PhysicalMotion) 또는3(PhysicalButtons)으로 응답한다. 버튼 프로필은 센서 flags를 위조하지 않으며 CALIBRATE는 실패 ACK다. 구형v1/v2 wire를 변경하지 않는다. 보호된 특성과 GATT UUID·wire는 protocol의 `spec/controller-v3.md` 및 `controller-v2.md`가 기준이다.

입력 TTL200ms/제어1000ms, 일시 전송 실패20ms 재시도, 이벤트3회 재전송과STATUS1회를 사용한다. 구독 변경·단절은 큐를 비우며 연결 세대가 다른 항목은 송신하지 않는다. 배터리255는 미측정이다.

## 40 소프트웨어 재현과 실물 인수

```sh
cmake -S . -B .work/build -DDT_PROTOCOL_ROOT=/absolute/checked/protocol -DCMAKE_C_FLAGS='-fsanitize=address,undefined'
cmake --build .work/build
ctest --test-dir .work/build --output-on-failure
```

보드 빌드 절차는 README와 `tools/build-board.py`, 실물 담당자의 배선·모델·축·로그·지표 인수는 `docs/imu-developer-handoff.md`와 중앙 docs의 `docs/guides/imu-handoff.md`를 따른다. 컴파일 전용 센서 fixture는 플래시하지 않는다. 초기 NU40 보드 정의는 `chcbaram/nu40dk-kit`의 고정 커밋 `df4367ed03d35c8599dc6007f049b2f4df9812b3`을 참고했으며 원래 MIT 고지를 보존한다. 원본Central 예제와 이Peripheral 앱의 역할은 다르다.

합성 궤적699/700, 일상동작500개 오검출0·중복0, 피크 뒤 판정150ms 중앙값/330ms 최대는 **합성 분류 실험**이며 무선 지연/사람 정확도가 아니다. 수치는 `docs/physical-events-validation.json`과 `tennis-gesture-simulator.md`에서 재현한다. NU54의 기존v2 실제 본딩·메시지 왕복·버튼은 확인됐고 반복 버튼 시험은 필요 없다. 새v3 실물flash/IMU·NU40 GPIO·Windows 실기는 미검증이다.

## DT-HW-31-10-00 · 적용 보드와 보안

보안/저장 설계 기록. 실제 제품은 **NU40-DK + 별도 외장 IMU**다. NU-54 시험 성공을 NU40 실기나 IMU 정확도로 표시하지 않는다.

| 대상 | 역할/SDK qualifier | GPIO alias | 저장 |
| --- | --- | --- | --- |
| NU40-DK | 제품 기본, nu40dk_kit/nrf52840 | confirm P0.31, start P1.7, pause P1.6, skill P1.5 | 기본 보드 NVS |
| NU-54 | 현재 연결된 센서 없는 임시 시험 보드, nu54v_dk/nrf54l15/cpuapp | vendored button0..3의 DT alias | Tennis 전용 ZMS0x168000..0x170000, 별도 영속 identity1 |

NU40 보드/극성은 [nu40dk-kit 고정 원본](https://github.com/chcbaram/nu40dk-kit/tree/df4367ed03d35c8599dc6007f049b2f4df9812b3/firmware/10_xbox/nu40dk-kit-xbox) DTS와 src/hw/driver/button.c의 _DEF_LOW를 따랐다. vendored DTS를 덮어쓰지 않고 애플리케이션 overlay에 GPIO_PULL_UP|GPIO_ACTIVE_LOW를 명시했다. 외장 IMU 핀은 아직 추정하지 않는다.

현재 화면 없는 라켓은 **LE Secure Connections + Just Works 암호화/본딩**을 사용한다. PIN/숫자 대조에 의한 최초 페어링 MITM 인증은 없다. 사용자가 NU54 Tennis 시험 기기에 명시적으로 승인했다. CONFIG_BT_SMP_SC_PAIR_ONLY와 BT_BONDABLE을 사용하며 authenticated Level4를 강제하는 SC_ONLY와 혼동하지 않는다. CCC와 보안 읽기는 암호화가 필요하며 명령·Notify는 현재 peer의 level>=2와 bond 저장을 추가 확인한다. 보호된4바이트 특성은 [protocol ICD](https://github.com/digital-tennis/digital-tennis-protocol/blob/12dae689fd9b25187426829c606529508c100e65/spec/controller-v2.md)를 따른다. 키·PIN·주소를 값에 넣지 않는다.

NU54 overlay는 현재 직접 실행 시험용이다. 원래 slot1의 사용하지 않는 꼬리32KiB를 Tennis 설정으로 사용하므로 MCUboot/OTA와 병용하지 않는다. 최초 설치 전 빈 영역인지 확인하며, 기존 지갑 저장0x174000..0x17d000은 읽거나 지우지 않는다. NU40에 이 저장 맵을 적용하지 않는다. NU54는 별도 영속 identity1을 만들어 원래 앱의 OS GATT 캐시·bond를 재사용하지 않는다. 기본 제품 NU40는 기본 identity0이다. 출처·원래 MIT 고지는 boards/nucode/nu54v_dk에 보존한다.

### NU54 승인된 코드 교체

`tools/install-nu54-preview.py`는 기본적으로 계획만 출력한다. 실제 쓰기는 별도 승인 후 --execute, 검토한 HEX SHA256, 새로운 비공개 --backup 디렉터리를 모두 지정해야 한다.0..0x30000의 코드만 허용하고 UICR/전체 erase/자동 unlock을 금지한다. 최초 설치는 Tennis 저장이 비어 있어야 한다. 이미 설치한 Tennis를 수정할 때는 --previous-image와 성공한 --previous-receipt를 함께 주고 기존 코드 바이트가 일치해야 한다. 코드 섹터만 백업/교체하고128바이트 블록으로 공개 HEX 코드 구간을 최종 대조한다. 지갑·bond 데이터는 공개하지 않는다. 백업·HEX는 Git에 넣지 않는다. 실물 리비전/다른 앱에 이 도구를 일반 플래셔처럼 사용하지 않는다.


## 60 과거 검증 기록

`bonding-build-validation.json` 등 이전v2 기록의 소스/프로필/성공 범위는 당시 근거로 유지한다. 현재v3 기본 경로의 성공으로 소급하지 않는다. 지갑/결제/프로비저닝 기능·자료는 본 제품에 포함하지 않는다.
