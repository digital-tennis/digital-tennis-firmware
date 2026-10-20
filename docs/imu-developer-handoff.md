# DT-HW-31-20-00 · 외장 IMU 개발 인수 가이드

## 00 문서 관리와 적용 대상

| 항목 | 내용 |
| --- | --- |
| 개정 / 기준일 | R04 / 2026-10-20 |
| 독자 | NU40-DK와 외장 IMU를 실제로 보유한 후속 개발자 |
| 제품 | **NU40-DK + 별도로 연결·고정하는 외장 6축 IMU** |
| 현재 시험 보드 | NU-54는 센서 없는 버튼/BLE 시험용. 이번 작업에서 추가 버튼 시험·flash 없음 |
| 확인 범위 | 소스/호스트 검사/컴파일 fixture. 실제 센서 배선·인식률은 미검증 |
| 계획 / 라이선스 | 원래 12일·3명·36PD 유지. 자체 코드·문서 Apache-2.0, 외부 보드 파일 원래 라이선스 유지 |

DT 번호와 절 구성은 프로젝트의 문서 관리 방식이다. ATA iSpec 2200 인증·원문 DTD 호환 문서라는 뜻이 아니다. 이 문서는 다른 개발자가 **어느 저장소의 어느 함수를 왜 고치는지**와 완료 증거를 인수하기 위한 것이다.

## 10 현재 구현과 저장소별 수정 위치

**현재 제품 경로는 v3 물리 이벤트다.** 펌웨어는6축에서 횡회전+/−·위→아래·짧은 회전·대각 하강/상승을 판정한다. 서브·스매시·발리·스킬의 의미는 게임이 결정한다. 보드에 경기 상태를 보내거나 경기 context를 설정하지 않는다. [현재 설계·함수별 수정 지도](physical-controller-events.md)가 이 문서의 기본 계약이다. 구형 v1/v2 샷 ID는 호환용으로만 유지한다.

`external_imu.c → dt_controller_tick → dt_gesture_classify_ring → v3 MOTION → SDK → 공통 adapter → PhysicalTennisInputResolver → SubmitInput`.

드라이버는 가속도m/s²·각속도deg/s·단조ms를 반환한다. `src/main.c`는10ms 절대deadline으로100Hz를 목표로 취득하고 지연된 주기를 건너뛴다. 실제 ODR·새샘플·fetch지연은 실측해야 한다. raw6축은 개발 UART/CSV로 기록하며 일반 BLE는20바이트 물리 이벤트다. 보드/센서 변경은 firmware, wire 추가는 protocol, 수신 gate는 SDK, 테니스 의미는 game에 구현한다. 구체적 함수·불변조건은 위 수정 지도에 있다.

## 20 시작 전에 실물 담당자가 작성할 표

아직 제품의 특정 IMU 모델이나 실제 배선을 확정하지 않았다. MPU6050/LSM6DSO 컴파일 예제는 선택 가능하지만 실물 검증된 배선은 아니다. 아래 빈칸을 채운 후 배선한다.

| 항목 | 기록할 값 |
| --- | --- |
| NU40-DK 리비전 / 보드 정의 | 보드 인쇄·회로도·현재 vendored DTS와 대조 |
| 센서 모델 / breakout 리비전 / 데이터시트 | 가속도·자이로 모두 지원하는지, Zephyr compatible |
| 전원·논리 전압 / 공통 GND | 보드와 센서의 허용 전압으로 판단 |
| I2C 또는 SPI | 실제 헤더 핀, pull-up/CS/INT, 다른 주변장치 충돌 |
| 주소 또는 CS / WHO_AM_I | 데이터시트 값과 실제 조회 값 |
| 취득 설정 | accel/gyro ODR, full-scale, 축 순서, 드라이버 반환 단위 |
| 장착 | 센서 XYZ 화살표, 라켓 손잡이 방향, 좌/우손, 고정 사진 |
| 실행 소스 | firmware/protocol/SDK/game SHA, NCS·컴파일러, 이미지 SHA |

`tests/compile-fixtures/mpu6050.overlay`는 **합성 컴파일 fixture**다. MPU6050이나0x68을 제품 선택으로 결정한 것이 아니며 배선도·설치용 파일이 아니다. 그대로 flash하지 않는다.

## 30 첫 과제: 드라이버에서 정상 샘플 받기

**상황:** BLE는 연결되지만 외장 센서의 alias가 없으면 `ready=false`다. 버튼 시험 성공을 센서 정상으로 바꾸면 안 된다.

1. 검토한 `overlays/` 파일에 센서 노드를 선언하고 `/aliases`의 `external-imu`를 그 노드로 연결한다. 실제 I2C/SPI pinctrl과 compatible은 담당자가 확인한 값으로 작성한다. 기본 보드 overlay는 `--overlay`로 추가하는 방식으로 보존한다.
2. 필요한 Zephyr 센서 드라이버 Kconfig를 활성화한다. `CONFIG_SENSOR`, I2C/SPI만 켜는 것으로 모든 센서 드라이버가 준비되는 것은 아니다.
3. `dt_external_imu_read()`에서 `device_is_ready`, `sensor_sample_fetch`, 두 XYZ 채널 읽기를 확인한다. 현재 Zephyr 경로는 accel을m/s²로 받고 gyro rad/s를deg/s로 변환한다. 전용 register 드라이버를 추가하면 원시 count→이 단위의 변환은 이 계층에서 한 번만 수행한다.
4. 드라이버가 지원하는 ODR/full-scale 설정을 초기화 경로에 추가한다. 지원하지 않는 `sensor_attr_set`을 성공으로 간주하지 않는다. 필요하면 data-ready/새 샘플 여부를 처리해 같은 측정치를 새 샘플로 반복 집계하지 않게 한다.
5. 실패하면 `false`를 반환한다. 이전 유효 값을 현재 시각으로 다시 보내거나 ready/calibrated를 강제로 켜지 않는다.

**관찰 예:** 정지 상태 가속도 크기는 약9.80665m/s², gyro는 정지 bias 주변이어야 한다. 특정 축에 +g가 나타나야 한다고 장착 확인 없이 단정하지 않는다. 회전 방향을 알고 한 축씩 움직여 부호와deg/s 스케일을 확인한다.

**완료 조건:** 센서 없음/분리/버스 오류에서 입력0, 정상 센서에서 유한한6축값·단조 시각, ODR와샘플 공백 측정 기록이 있다. 두 예제는 polling이므로 실제 data-ready/하드웨어 timestamp 보장은 별도로 확인한다. 아직 포핸드 인식 성공을 주장하지 않는다.

## 40 두 번째 과제: 보정·장착 행렬·구간

`src/main.c`는 `dt_motion_physical_defaults()`를 주입한다. 기본axis는identity이고 실제 장착에 맞는 정규직교 행렬(det+1)을 설정해야 한다. 좌손 처리는 축 변환 후 angular-vector Y/Z를 반전한다. 게임의 Near/Far와 이중 반전하지 않는다.

현재 빌드 설정은 `config/gesture-detector.csv`의filter_alpha0.4·trigger180/release80deg/s와 `gesture-rules.csv`의6종 특징 범위다. 수정 후 CMake 재빌드가 필요하며 런타임 BLE 설정은 없다. calibration_samples64, cooldown200ms, max_gap100ms, min/max20/1000ms 등의 설정은 `dt_motion_config`에 있다. 정지 조건gyro≤15deg/s·가속도크기와g 차이≤1m/s²·제한5초는 현재C코드 값이다. 실물 조정이 필요하면 검증된 설정 필드로 옮긴다.

PC의 CALIBRATE→연속정지→성공ACK+실제flags를 확인한다. 샘플 공백/오류는 보정과 진행 중 구간을 무효화한다. 상대각은 해당 구간의 회전량이며 전역위치·절대heading이 아니다. 일반 제품은 구간 종료까지 기다려 물리 이벤트를 낸다. 지연을 줄이려면 별도 실험으로 정확도/중복/위상 전달 계약부터 검토한다.

## 50 세 번째 과제: 물리 패턴 튜닝

[가상 실험실](../simulator/README.md)의 두 위→아래 궤적은 모두kind3다. 같은 센서 파형을 경기 상태에 따라 다르게 라벨링하지 않는다. 물리6종과 정지·걷기·회전 없는 충격·손떨림 등 음성 표본을 사용자/세션별로 나누고 실측 CSV를 확보한다.

CSV 열은 `time_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps`다. 정답 label은 별도 메타데이터에 두며 C 입력에는 포함하지 않는다. UART trace는 [기록/재생](air-band-imu-adaptation.md#40-실행기록재생-과제)을 따르고 누락을 임의 보간하지 않는다. `imu_replay --physical <csv>`로 같은 C 분류기를 실행한다. `dt_gesture_classify_ring`의특징과CSV 범위를 조정하고 미일치/겹침은 입력을 만들지 않게 한다. 세부 CLI는 인자 없이 실행한 `imu_replay`의 usage 또는 simulator/run.py를 확인한다.

완료 증거는6종 정밀도/재현율·오검출/분·누락/중복·구간/무선 지연 분포·장착/사용자/속도/양손별 실패 사례다. 합성 정확도는 실물 정확도와 구분한다. 과거7종/경기context 임계값 보고서는 당시 커밋의 연구 기록으로 보존하며 현재 게임 샷 계약으로 사용하지 않는다.

## 60 PC·Unity 인수

1. NU40은 PhysicalMotion=2, 센서 없는 NU54는 PhysicalButtons=3이다. LINK_PROBE는v2이며 새펌웨어응답은v3다. 기존v2.kind0/1기기도 SDK가 별도로 지원한다.
2. 검색→선택→OS보호연결/본딩→실제왕복→모션프로필의정지보정/ACK→물리입력테스트→저장 순서다. 버튼전용은센서보정없이번호별Down/Up을시험한다.
3. SDK는 power/100, yaw/pitch 각각/4500 후[-1,1] clamp와 PC수신시각/연결세대를 제공한다. raw샘플 재분류나 테니스샷 선택을 하지 않는다.
4. 같은kind3를 자기서브/랠리높은공/낮은공에서 각각 Serve/Smash후보/Height거부로 확인한다. 짧은회전은 바운드전Volley/후일반타격이다. 실제적중은기존거리·높이·중복·타이밍gate가결정한다.
5. 온라인게스트는물리종류그대로전달하고호스트가경기상태로해석한다. 끊김/소스변경은이전입력폐기,설정테스트는경기에누출되지않아야한다.

## 70 장비 없이 가능한 검사와 보드 빌드

각 저장소 root에서 실행한다. 경로는 실제 clone 위치로 바꾼다.

```sh
# firmware: clone 위치를 실제 경로로 바꾼다.
cd /absolute/path/digital-tennis-firmware
cmake -S . -B .work/imu-host -DDT_PROTOCOL_ROOT=/absolute/path/digital-tennis-protocol
cmake --build .work/imu-host
ctest --test-dir .work/imu-host --output-on-failure

# protocol: C/C# 공유 벡터·오류 검증
cd /absolute/path/digital-tennis-protocol
python3 tools/check.py

# BLE SDK: 실물 없는 패킷/연결/보정 검사
cd /absolute/path/digital-tennis-ble-unity
dotnet run --project tests/Checks.csproj -c Release -p:ProtocolRoot=/absolute/path/digital-tennis-protocol

# game: 실제 프로젝트 위치에서, Editor를 닫고 실행
cd /absolute/path/digital-tennis-game
bash tools/unity.sh check
bash tools/unity.sh test-controller
```

보드 빌드는 firmware root에서 수행한다. `--overlay`에는 담당자가 실제 센서에 맞춰 검토한 파일을 전달한다.

```sh
python3 tools/build-board.py \
  --sdk-root /absolute/ncs/v3.4.1 \
  --toolchain /absolute/nordic/toolchain \
  --board nu40dk_kit/nrf52840 \
  --protocol /absolute/path/digital-tennis-protocol \
  --overlay overlays/your-reviewed-sensor.overlay \
  --out .work/nu40-your-sensor
```

이 도구는 **컴파일만** 한다. 다른 사람이 가진 보드를 자동 발견하거나 쓰지 않는다. 실제 설치는 그 담당자가 보드 리비전·디버거·이미지와 기존 앱/저장영역을 확인한 후 적합한 공식 절차로 진행한다. NU54 임시 설치 도구를 NU40 일반 플래셔로 사용하지 않는다.

호스트 검사에 추가할 핵심 회귀는 NaN/Inf·범위 초과, 축 회전/좌손, 보정 중 움직임·시간초과·분리, 시각 공백/wrap, 너무 짧은/긴 동작, cooldown·한 동작 중복, 모호한 규칙과 미일치다. 테스트의 mock read는 하드웨어 검증과 별도 근거다.

## 80 장애 격리와 인수 완료

| 증상 | 먼저 확인할 계층 |
| --- | --- |
| BLE 미검색 | 전원·광고·OS 권한·native helper. IMU 분류부터 바꾸지 않기 |
| 연결만 성공, ready=false | alias/compatible/Kconfig, device_is_ready, sample_fetch 반환값 |
| 보정이 계속 실패 | 센서 단위·정지 조건·연속 샘플·ODR·5초 제한 |
| 보정 성공, MOTION 없음 | 회전 trigger/release·구간 길이·cooldown·6종 범위 미일치/겹침 |
| SDK가 이벤트 거부 | Motion profile, 현재 보정 ACK, flags, CRC·중복·sequence·현재 연결 epoch |
| UI 테스트는 통과, 경기 공이 안 맞음 | 선택 source·게임 phase·거리/높이·현재 shot/rally ID·receipt time |
| 횡회전+/− 뒤집힘 | 센서 장착 좌표·axis·left_handed. 게임 Near/Far를 장치에서 반전하지 않기 |

인수 PR에는 실제 배선표·장착 방향·소스/도구/이미지 해시, 진단 CSV 설명, 호스트 결과, 실제 동작별 혼동 행렬, 일반 Unity 설정/서브/타격/스킬 결과와 미검증 목록을 포함한다. 센서를 보유한 개발자가 수행하지 않은 결과는 빈칸/미검증으로 둔다. 코덱 변경이 있다면 protocol→firmware·SDK→game→문서 순서로 참조를 고정한다.

## 과거 실험과 현재 계약

[임계값 실험](threshold-experiment.md)의7종/693/700 등은 이전커밋의 고정 연구 기록이다. 현재 물리6종은 기존 수치 범위를 유지하면서 서브/스매시 두 label을 하나로 합쳤다. 같은 파형의 분류가 게임 상태에 의존하지 않아야 한다. 실물 담당자는 새6종 label로 기록/평가하고 보드 기본이 v3임을 명시한다.
