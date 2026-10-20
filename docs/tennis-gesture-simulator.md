# DT-HW-31-22-00 · 테니스 모션 추론과 가상 IMU 검증

> 이전 커밋의 실험/적용 기록입니다. 현재 보드 기본·6종 물리 이벤트·수정 위치는 [v3 설계](physical-controller-events.md)를 우선합니다. 당시7종·경기context·피크 기본 설명은 현재 계약이 아닙니다.

> 아래 결과 표는 초기 기능 커밋 `b0db078`의 R01 기록입니다. 최신 CSV 임계값 선택·같은 최종 표본 전후 비교는 [임계값 실험](threshold-experiment.md)을 따릅니다.

## 00 적용 대상과 판정 범위

R01 / 2026-10-20. **NU40-DK + 별도 부착 외장 6축 IMU**가 제품 타깃입니다. NU54는 기존 센서 없는 BLE/버튼 시험용입니다. 이번 작업에는 flash·실물 IMU·추가 버튼 시험이 없습니다. 최초 12일·3명·36PD 계획은 유지합니다. DT 번호는 ATA iSpec 2200을 참고한 자체 문서 관리 형식이며 공식 적합성을 뜻하지 않습니다. 자체 구현·문서는 Apache-2.0입니다.

드럼 스틱의 아래 방향 가속도 피크처럼, 테니스에서도 **방향·회전 범위·시간·가속도**에서 입력 의도를 추론할 수 있습니다. 다만 센서가 모든 테니스 규칙을 관찰할 수 있는 것은 아닙니다. 그래서 가상 궤적과 C 판정기를 분리하고, 구분할 수 없는 경우도 검증합니다.

**이번 결과는 실험용 7종 의도 분류와 재현 가능한 시뮬레이터입니다. 기본 보드 앱의 자동 판정은 기존 빠른 피크 프로필입니다.** 새 프로필은 별도로 선택하며, 경기 상황을 BLE로 보드에 전달하는 기능은 아직 없습니다. 현재 Unity 실물 7종 플레이가 완성됐다는 의미가 아닙니다.

## 10 각 동작에서 추론하는 것

고정된 그립의 중립 자세에서 X는 오른쪽, Y는 네트 쪽/라켓 손잡이→헤드, Z는 위인 오른손 좌표계를 사용합니다. 세계축과 라켓축은 중립에서만 일치하며 움직일 때 센서값은 라켓에 붙은 축으로 표현됩니다. 왼손과 장착 회전은 별도 변환합니다.

| kind / 이름 | 가상 궤적과 C 특징 | 구분의 한계 |
| --- | --- | --- |
| 1 포핸드 | 준비 뒤 +Z 방향의 큰 횡회전 | 오른손 기준. 실제 그립·양손 스트로크는 녹화 자료로 확인 필요 |
| 2 백핸드 | 반대인 −Z 방향의 큰 횡회전 | 손잡이의 3D 위치를 복원한 결과가 아님 |
| 3 서브 | 머리 위에서 앞으로 내려오는 −X 회전 + **서브 대기 상황** | 같은 파형의 스매시와 IMU만으로 구분 불가 |
| 4 스매시 | 위에서 내려오는 −X 회전 + **랠리 상황** | 공의 위치·높이·적중 여부는 게임이 판정 |
| 5 발리 의도 | 작은 회전 범위의 짧은 전방 펀치 | 실제 발리는 바운드 전에 치는 샷. 바운드는 센서가 볼 수 없음 |
| 6 슬라이스 의도 | 횡회전과 아래로 자르는 −X 성분 | 라켓 궤적 후보이며 실제 공의 백스핀을 측정하지 않음 |
| 7 로브 의도 | 횡회전과 위로 들어올리는 +X 성분 | 실제 공이 상대 위로 넘어가는지는 게임 물리의 결과 |

스킬(kind8)은 테니스의 표준 스트로크가 아닙니다. 기존 명시적 버튼/모드를 유지하며 이 7종 자동 판정과 섞지 않습니다. 경기의 타격 가능 거리·높이·타이밍·서브 순서·스킬 재고는 계속 게임이 결정합니다.

## 20 가상 센서가 값을 만드는 방법

`simulator/physics.py`는 판정 규칙 CSV를 읽지 않습니다. `simulator/motions.csv`의 준비각·마무리각·준비/정지/전진 시간·전방 이동량에서 5차 smoothstep 궤적을 만듭니다. 초기 1.2초는 정지 보정 구간, 리셋은 느린 복귀입니다. 생성 동작 이름과 기대 kind는 평가용 메타데이터에만 존재합니다.

```text
p_sensor(t) = p_pivot(t) + R_body(t) × offset
R_sensor(t) = R_body(t) × M_mount
accel_sensor = transpose(R_sensor) × (second_derivative(p_sensor) − gravity)
gyro_sensor = SO(3) log(transpose(R_sensor(t−h)) × R_sensor(t+h)) / (2h)
```

가속도는 m/s², gyro는 deg/s로 변환합니다. `h=1ms` 수치 미분, 출력은 100Hz입니다. 센서가 회전 중심에서 0.25m 떨어진 모델로 접선·구심 가속도와 중력을 계산합니다. 인식을 맞추기 위한 가짜 공 충격 피크를 추가하지 않습니다. 독립적인 정지 중력·일정 병진 가속도·등속 회전의 해석값과 비교하는 단위 검사가 있습니다.

기본 잡음은 accel 표준편차0.02g, gyro1.5deg/s에 배율을 곱하고, gyro 고정 bias는 축마다 ±1deg/s입니다. ±16g/±2000deg/s 범위로 포화·16비트 양자화하며 잘린 축 수를 기록합니다. 이는 가상 센서 가정이며 실제 IMU 성능 사양이 아닙니다. 검증은 속도±25%, 범위±15%, 잡음0.3..1.3배, 센서 offset0.20..0.30m, 좌/우손과 Z 장착0/90/180/270도를 섞었습니다. 모든 3D 장착을 검증한 것은 아닙니다.

왼손 궤적은 위치와 회전을 반사하여 만듭니다. 각속도는 축벡터이므로 C에서는 장착 회전 후 Y/Z를 반전해 오른손 기준 특징으로 비교합니다. 기존 패킷 조준용 상대각 경로의 Z 반전과 특징용 raw 변환은 별도입니다. 장착 행렬에는 반사를 넣지 않고 정규직교·determinant +1인 회전만 허용합니다.

## 30 C 판정 경로와 설정

```text
7열 CSV → dt_imu_port → 실제 정지 보정 → dt_controller_tick
  → 자이로 시작/해제 → 최근 raw 128샘플의 마지막 연속 회전 구간
  → dt_gesture_classify_ring → 후보/경기 상황/모호함 검사
  → 기존 dt_packet/20바이트 코덱
```

시작/해제는 기존 필터·220/80deg/s 히스테리시스를 사용합니다. raw 회전 구간에서 gyro bias·장착 회전·좌/우손을 처리한 뒤 회전 경로 길이, 부호 있는 X/Z 적분/경로 길이, 방향 일관성, 지속 시간과 중력 포함 가속도 피크를 계산합니다. `config/gesture-rules.csv`의 범위에 모두 들어간 후보를 모읍니다. 서브 대기는 스매시 후보를 제외하고 랠리는 서브 후보를 제외합니다. 남은 후보가 여러 개거나 없으면 입력을 만들지 않습니다. 상황 모름에서 overhead 후보3+4는 거부합니다.

CSV는 CMake 설정 단계에 `tools/generate-gesture-rules.py`가 유효성을 검사하고 생성 헤더로 변환합니다. NaN/Infinity·누락·중복 kind·잘못된 범위는 빌드를 실패시킵니다. 이 CSV는 **빌드 시 설정**입니다. 외부 파일을 게임 실행 중 다시 읽는 밸런스 CSV와 다릅니다. 임계값을 바꾸고 재빌드하면 됩니다.

기본 앱의 논리 보관 길이는20개, 실험 프로필은128개입니다. 자료형의 버퍼는128개이므로 일반 빌드도 그만큼의 정적 RAM을 할당합니다. 분류는 ring을 직접 읽어 MCU 4096바이트 main stack에 샘플 전체를 복사하지 않습니다. 스윙 해제까지 기다리므로 가속도 피크 이후 판정 지연이 있습니다. 현재 가상 평가의 중앙값170ms·최대360ms이며 **CPU 실행 시간이나 BLE·Unity 전체 지연은 아닙니다**. 실시간 타격 기본값으로 자동 배포하지 않습니다.

## 40 실행 과제와 기대 결과

[실행 안내](../simulator/README.md)에 따라 빌드/서버를 시작합니다. 브라우저가 없는 환경에서는 다음처럼 실행합니다.

```sh
python3 simulator/run.py --motion forehand --context rally --out .work/fh.json
python3 simulator/run.py --motion serve --context unknown --out .work/overhead-unknown.json
python3 simulator/run.py --motion serve --context serve --out .work/serve.json
python3 simulator/run.py --motion serve --context rally --out .work/smash-context.json
python3 simulator/run.py --motion slice --context rally --left --mount 90 --out .work/slice.json
python3 simulator/evaluate.py --repetitions 100 --validation-seed 731091 --out .work/motion-validation.json
```

**기대 동작:** 포핸드kind1, 상황 모름 overhead는 입력0/후보3+4, 같은 센서값을 서브 상황에 주면kind3·랠리에 주면kind4입니다. 왼손90도 장착의 슬라이스는kind6입니다. 정지·걷기·병진 흔들림·손 떨림·라켓 돌리기는 현재 표본에서 입력0입니다. 오른손 pure Z 회전은 Z 장착 회전만 바꿔도 동일하므로 잘못된 장착의 반례로 쓰지 않습니다. 두 축이 있는 슬라이스에서 잘못된 장착 행렬을 주어 차이를 확인합니다.

실제 녹화 CSV도 같은 C 실행기에 넣을 수 있습니다.

```sh
.work/motion-simulator/imu_replay --gestures --diagnostics --context=rally samples.csv > decisions.csv
```

입력 열은 정확히 `time_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps`입니다. 첫65개 이상은 정지여야 합니다. 왼손은 `--left`, 장착은 `--axis=1,0,0,0,1,0,0,0,1`처럼 실제 센서→라켓 회전을 지정합니다. 파일에 label을 추가하지 않습니다. 출력 후보는 `1 << kind` 비트마스크이고 kind0 진단에는 패킷이 없습니다. reason0성공/1짧음/2미일치/3겹침/4상황필요/5유효하지않음입니다.

## 50 검증 결과를 읽는 법

준비 시드1701의 동작84개·일상 동작60개와 분리한 고정 평가 시드731091을 사용했습니다. 규칙과 궤적 CSV 해시를 고정한 뒤 평가했으며 누락15개에 맞춰 임계값을 재조정하지 않았습니다.

| 기대 종류 | 맞게 출력 / 표본 | 미인식 | 다른 종류로 오분류 |
| --- | --- | --- | --- |
| 포핸드 | 99 / 100 | 1 | 0 |
| 백핸드 | 100 / 100 | 0 | 0 |
| 서브 | 100 / 100 | 0 | 0 |
| 스매시 | 100 / 100 | 0 | 0 |
| 발리 | 86 / 100 | 14 | 0 |
| 슬라이스 | 100 / 100 | 0 | 0 |
| 로브 | 100 / 100 | 0 | 0 |
| 정의한 일상 동작 5종 | 입력0 / 500 | 해당 없음 | 오검출0 |

총685/700(97.9%)은 **정의한 가상 궤적 분포에서의 일치율**입니다. 약한 발리는 필터 후 시작 임계값220deg/s를 넘지 않는 사례가 있고 포핸드 누락은 가속도1.59354g가 규칙 하한1.6g보다 작았습니다. 부정 표본500개에는 이미 알려진 테니스 파형과 완전히 같은 우발적 움직임은 포함하지 않았습니다. 별도 반례는 기대 메타데이터를0으로 바꾸어도 동일한 포핸드 센서값이kind1로 나옴을 확인합니다. 센서만으로 사용자 의도를 읽을 수는 없습니다.

호스트6개 검사군(물리/연동9개 포함), ASan/UBSan6개 검사군과 NU40 sensorless/MPU6050/LSM6DSO, NU54 buttons 컴파일을 통과했습니다. 기존 C# 코덱/SDK에는 실제 C 생성7종 패킷을 메모리 transport로 전달합니다. 실물 BLE·Unity 플레이와 구분합니다. [구조화 검증 기록](gesture-simulator-validation.json)에 해시·실패 표본·재현 명령을 보관합니다.

## 60 후속 개발자가 수정할 위치

| 변경하려는 것 | 파일/함수 | 제출할 증거 |
| --- | --- | --- |
| 실제 센서 취득/ODR/배선 | `src/hw/driver/external_imu.c`, 검토한 overlay | [실물 인수 표](imu-developer-handoff.md), 정지/단축 회전 로그 |
| 새 가상 움직임·속도·그립 모델 | `simulator/motions.csv`, `physics.py:make_pose/inertial` | 궤적 시각화, 물리 해석값 비교, 판정 규칙과 독립 유지 |
| 7종 특징/후보 추출 | `src/ap/dt_gesture.c:dt_gesture_classify_ring` | 별도 사람 평가 세트, 종류별 누락/오검출·혼동 행렬 |
| 판정 범위 | `config/gesture-rules.csv` | 조정/평가 데이터 분리, frozen 해시, 재빌드 기록 |
| 프로필 선택·취득 버퍼·스윙 gate | `dt_controller.c:dt_motion_gesture_defaults/classify`, `src/main.c` | 실제 CPU 시간·정적RAM·전체 지연·재연결 회귀 |
| 경기 상태 전달 | `dt_controller_set_context`가 C 접점. **현재 BLE 명령 없음** | protocol에 버전/상태/순서/ACK·재연결 초기화 설계 후 firmware→SDK→game 구현 |
| 기존 이벤트 수신/게임 효과 | protocol v2→BLE SDK→`NativeControllerBackend`→`SubmitInput` | 기존 본딩/보정/중복 gate와 경기 규칙 통과 |

실험용 profile 선택 예는 다음과 같습니다. 정상 센서 포트와 정지 보정 흐름을 그대로 사용합니다.

```c
dt_motion_config config = dt_motion_gesture_defaults();
// config.axis에 실제로 확인한 센서→라켓 회전을 넣는다.
dt_controller_init(&controller, imu_port, config);
// 아래 호출은 로컬 C 예제. BLE로 이 상태를 받는 경로는 아직 없다.
dt_controller_set_context(&controller, DT_CONTEXT_SERVE);
// 매치 상태 전환 때 한 번 호출. 매 tick 호출하면 진행 중 스윙을 취소한다.
```

상황 변경은 진행 중 스윙을 취소하고 release 뒤 재무장합니다. 끊김·새 경기·수신 상태 만료 시 UNKNOWN으로 되돌리는 책임까지 함께 설계해야 합니다. 무선 명령을 추가할 때 SWING 페이로드에 상태를 몰래 끼우지 않습니다. 현재3종 빠른 프로필/특수샷 명시 모드는 그대로 비교 대상으로 유지합니다. 사람·그립·속도·좌우손·센서 장착의 독립 평가와 지연 예산이 확보된 뒤 기본 프로필 전환 여부를 결정합니다.

## 70 근거와 제외 범위

- [Air Band 고정 소스](https://github.com/hex-aragon/air-band/tree/d89ce32a460b34c0cc740235dc9a76e74396a094), [스틱 영상](https://www.youtube.com/watch?v=T8Lj7S-T7LY), [제작자 설명](https://next-ai-agent.tistory.com/m/8): 움직임→센서→액션 개념 참고. 라이선스 없는 원본 코드/자산을 복사하지 않았습니다.
- [ITF 용어](https://www.itftennis.com/en/about-us/organisation/tennis-glossary/): 발리·로브·슬라이스·스매시의 실제 의미와 가상 의도를 구분했습니다.
- [Ebner·Findling의 테니스 스트로크 연구](https://ambientintelligence.aalto.fi/paper/Tennis_Stroke_Recognition.pdf): 실제 사람/센서 위치에 따른 데이터 수집과 구간 특징 평가 접근을 참고했습니다. 논문 모델·코드를 사용하거나 논문의 성능을 본 결과로 인용하지 않습니다.

가상 모델에는 손목/팔꿈치 관절, 그립 변화, 피부 움직임, 실제 공 충격·스핀·바운드, 센서 시계 드리프트/실측 손실, 무선 전파와 Unity 프레임 지연이 없습니다. 사람이 휘두르는 실제 데이터와 실시간 성능을 후속 담당자가 검증해야 합니다.
