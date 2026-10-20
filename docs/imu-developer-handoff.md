# DT-HW-31-20-00 · 외장 IMU 개발 인수 가이드

## 00 문서 관리와 적용 대상

| 항목 | 내용 |
| --- | --- |
| 개정 / 기준일 | R03 / 2026-10-20 |
| 독자 | NU40-DK와 외장 IMU를 실제로 보유한 후속 개발자 |
| 제품 | **NU40-DK + 별도로 연결·고정하는 외장 6축 IMU** |
| 현재 시험 보드 | NU-54는 센서 없는 버튼/BLE 시험용. 이번 작업에서 추가 버튼 시험·flash 없음 |
| 확인 범위 | 소스/호스트 검사/컴파일 fixture. 실제 센서 배선·인식률은 미검증 |
| 계획 / 라이선스 | 원래 12일·3명·36PD 유지. 자체 코드·문서 Apache-2.0, 외부 보드 파일 원래 라이선스 유지 |

DT 번호와 절 구성은 프로젝트의 문서 관리 방식이다. ATA iSpec 2200 인증·원문 DTD 호환 문서라는 뜻이 아니다. 이 문서는 다른 개발자가 **어느 저장소의 어느 함수를 왜 고치는지**와 완료 증거를 인수하기 위한 것이다.

## 10 현재 구현: 어디에서 액션을 판정하는가

```text
외장 IMU → external_imu.c → dt_imu_sample
  → dt_controller_tick: 영점·축 변환·필터·스윙 구간·classify
  → dt_packet: SWING(kind/power/상대 yaw·pitch)
  → BLE TX Notify → protocol 코덱 → BLE SDK 검증/정규화
  → NativeControllerBackend → 입력 라우터/adapter
  → TennisMatchController.SubmitInput → 타격·서브·스킬·점수 규칙
```

**현재 제스처의 1차 판정은 펌웨어에서 한다.** PC는 raw 6축 샘플을 모아 포핸드/백핸드를 재분류하지 않는다. PC에서는 메시지의 연결·보정·중복·시각을 검사하고, 경기 상황에서 그 타격을 허용할지 판정한다. Bluetooth HID 마우스를 흉내 내거나 키보드 키를 합성하지 않는다.

현재 앱은 `src/main.c`의 10ms 절대 deadline으로 100Hz를 목표로 취득한다. 지연된 주기를 건너뛰며 따라잡기용 중복 fetch를 하지 않는다. 실제 취득 주기가 정확히100Hz라는 실기 증거는 아니며 fetch 시간·ODR·스케줄링을 측정해야 한다. 기본 앱은 회전 구간 안의 가속도 피크를 다음 샘플로 확인해 입력을 만들고, 원래 자이로 release 방식은 명시적 비교 프로필로 유지한다. [Air Band 분석·센서 예제·기록/재생](air-band-imu-adaptation.md)을 먼저 확인한다. 일반 경기에는 **v2 20바이트 이벤트**를 보내고 raw 샘플은 전송하지 않는다. 이는 기본 ATT MTU23의 value20에 맞춘 설계이며 BLE가 항상20바이트만 보낼 수 있다는 뜻은 아니다. 20바이트를100Hz로 보내면 값만2,000B/s, 2스윙/s를3회+STATUS1회 보내면140B/s다. 둘 다 링크 헤더·암호화·손실 비용을 제외한 비교다.

### 10.10 수정 위치 지도

| 요구 변경 | 저장소 / 파일 / 함수 | 책임과 주의점 |
| --- | --- | --- |
| 센서 모델·버스·주소·핀 | firmware `overlays/<검토한-센서>.overlay`, 보드 DTS, `prj.conf` | 실물 데이터시트와 핀맵으로 결정. NU54 저장/GPIO 설정을 NU40에 복사하지 않기 |
| 드라이버·ODR·full-scale·read 실패 | firmware `src/hw/driver/external_imu.c` / `dt_external_imu_read` | 완성된 가속도m/s²·자이로deg/s·단조 시각ms를 반환 |
| 샘플/설정 자료형 | firmware `src/common/dt_imu.h` / `dt_imu_sample`, `dt_controller.h` / 설정·규칙 | raw 배열 순서와 단위 고정. BLE 바이트 배치와는 별개 |
| 장착 방향·필터·임계값 | firmware `src/ap/dt_controller.c` / `dt_motion_defaults`, `dt_controller_init`, `dt_controller_tick` | `main.c`에서 실측 설정을 주입. 초기 기본값을 검증된 상품 값으로 간주하지 않기 |
| FH/BH/서브 분류 | firmware 같은 파일의 `classify` | 기본은 peak_y와 peak_z 기반 휴리스틱. 녹화 데이터로 확인·교체 |
| 특수 제스처·프로필 | firmware `dt_controller_set_rules`, `dt_controller_set_mode`, `dt_controller_intent` | 최대8규칙, 모호한 겹침/미일치 거부. 명시적 모드 선택과 자동 인식 구분 |
| 전송 큐·보안·재연결 | firmware `src/ap/ble_peripheral.c`, `dt_tx_queue.c`, `src/main.c` | 제스처 알고리즘 변경을 이유로 본딩/암호화나 연결 세대를 우회하지 않기 |
| 새 메시지/필드/샷 ID | protocol `spec/controller-v2.md`, `Runtime/ControllerPacket.cs`, `c/include/dt_protocol.h`, `c/src/dt_protocol.c`, `test-vectors.json` | `PacketCodec`은 `ControllerPacket.cs` 안에 있다. C/C# 및 공유 벡터를 함께 변경 |
| 수신·보정·중복·단위 변환 | ble-unity `Runtime/BleControllerClient.cs` / `Calibrate`, `Tick`, `TryRead` | 같은 연결의 보정 완료 ACK 필요. STATUS flags만으로 보정 성공 처리 금지 |
| 의도→게임 액션 | game `Assets/Scripts/Runtime/NativeControllerBackend.cs` / `TryRead` | SwingKind→PlayerAction 변환. 기기에 Near/Far 역할을 넣지 않기 |
| 선택한 기기·설정 테스트 | game `ControllerSetupSession.cs`, `ControllerDevices.cs`의 `InputDeviceManager`, `PlayerInputAdapter.cs` | 무선 연결·보정·서브/스윙/스킬 테스트 완료 후 저장. 저장 전 경기 입력 금지 |
| 거리·타이밍·물리 효과 | game `TennisMatchController.cs` / `ApplyPlayerInput`, `SubmitInput`, `RacketInput.cs`의 `RacketInputGate`·`RacketShot` | 일반 타격 게이트를 유지. 실제 인식 성공과 경기 타격 성공은 다른 지표 |

새 센서 모델을 붙이는 일만으로 protocol·SDK·game을 모두 수정할 필요는 없다. 기존 단위와 SWING 계약을 만족하면 펌웨어 변경으로 끝난다. 반대로 새로운 액션 ID나 전송 필드가 필요하면 protocol부터 의존성 순서대로 수정한다.

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

## 40 두 번째 과제: 보정·장착 행렬·스윙 구간

`dt_motion_config`는 `main.c`의 `dt_controller_init()`에 주입한다. 앱은 `dt_motion_peak_defaults()`를 사용한다. 기존 `dt_motion_defaults()`는 자이로 release 비교 프로필이며 아래 값은 실측 전 소프트웨어 시험 시작값이다.

| 설정 | 현재 시작값 | 바꿀 때 확인 |
| --- | --- | --- |
| axis | 3×3 identity | 센서→라켓 회전 행렬. 정규직교와 determinant +1을 init이 검사. 실제 양의 회전으로 부호 검증 |
| left_handed | false | 기존 조준 경로는 매핑된 gyro Z 반전. 실험 특징 경로는 축벡터 Y/Z 반전. 장착 행렬과 이중 반전 금지 |
| filter_alpha | 0.25 | 지연과 노이즈 절충. 0초과1이하 |
| trigger/release | 220/80deg/s | 시작/해제 히스테리시스. release<trigger |
| serve_dps | 180deg/s | 기본 분류의 최고 gyro Y 기준. 정확한 서브 판정이 보장되지 않음 |
| calibration_samples | 64 | 연속 정지 샘플. 움직이면 표본을 다시 수집 |
| cooldown_ms | 200 | 한 번 휘두른 뒤 재인식 간격 |
| max_gap_ms | 100 | 초과 공백에서 보정·스윙 무효화 |
| min/max_swing_ms | 20/1000 | 너무 짧거나 오래 지속되는 스윙 거부 |
| max_power_dps | 2000 | 기존 release 프로필의 최고 각속도→1..100 세기 스케일 |
| acceleration_peak | 앱 true, 비교 프로필 false | gyro 회전 구간 + accel 피크 확인 여부 |
| trigger_g / release_g / max_power_g | 2.5 / 1.5 / 12g | 중력을 포함한 accel 크기, 피크/재무장/세기 범위. peak 모드 세기는 accel 기반 |

현재 정지 보정 조건은 gyro 크기15deg/s 이하·가속도 크기와g의 차이1m/s² 이하, 제한5초다. 이 수치들은 `dt_controller_tick()`에 직접 작성돼 있다. 센서별 조정이 필요하면 검증 범위가 있는 `dt_motion_config` 필드로 옮기고 잘못된 설정을 거부하는 호스트 검사를 추가한다. 런타임 원격 설정·CSV 읽기는 **현재 펌웨어 기능이 아니다**. 게임의 CSV 설정과 혼동하지 않는다.

보정은 PC의 CALIBRATE 명령→연속 정지 샘플→ACK 성공/실제 flags의 흐름이다. 전역 heading/3D 위치 추적을 하지 않는다. yaw/pitch는 스윙 구간의 상대 회전량이며 장시간 적분한 위치가 아니다. 가속도는 샘플 유효성·정지 검사와 앱의 피크/세기 판정에 쓰이고, 동작 종류 분류는 주로 자이로를 사용한다. 피크 모드는 회전 시작부터 피크까지의 상대각을 보내며 평행이동만으로는 타격을 만들지 않는다.

## 50 세 번째 과제: 실제 모션을 액션으로 판정하기

별도 [가상 IMU·7종 실험 프로필](tennis-gesture-simulator.md)이 추가됐습니다. 3D 궤적→센서값→같은 C 코드로 분류하고 애매한 상황을 거부합니다. 현재 기본 앱은 기존 피크 프로필이며, 실험용 profile의 경기 상태 C setter에 대응하는 BLE 명령은 아직 없습니다. 실물 검증과 지연 측정 전 자동 적용하지 않습니다.

**입력:** 같은 장착 방식으로 녹화한 정지, 이동, FH, BH, 서브, 의도하지 않은 움직임. 가능하면 서로 다른 개발자·속도·좌/우손을 포함한다. 데이터를 학습/조정용과 평가용으로 분리한다.

재생 도구의 CSV 열은 `time_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps`다. session/label은 별도 메타데이터에 기록하고 재생기에 넣을 CSV에는 열을 추가하지 않는다. PC 주소·본딩 키·개인정보는 넣지 않는다. `--trace` 개발 빌드는 최대32행 큐/별도 UART 태스크로 원본6축을 기록한다. `tools/imu-trace-to-csv.py`로 명시 단위 CSV를 만들고 `imu_replay` 호스트 도구로 재생한다. [형식·정지 prefix·명령](air-band-imu-adaptation.md#40-실행기록재생-과제)을 따른다. 큐가 가득 차면 행을 버리며 시각 공백을 보간하지 않는다. BLE raw 스트리밍 기능과는 다르다. 녹화 자료는 동의받고 공개 가능한 범위만 저장한다.

**구현할 동작:**

1. 외장 센서 입력을 `dt_imu_port.read` fixture로 재생하고 피크와 다음 확인 샘플에서 한 이벤트만 나오고 release/rearm 후에만 다음 이벤트가 가능한지 확인한다. 비교 release 프로필에서는 release까지 확인한다.
2. 단순 세 동작이면 `classify()`의 기본 분류를 실측 특성으로 개선한다. 최고 Z의 부호만으로 모든 사람의 FH/BH가 구분된다고 가정하지 않는다.
3. 각도·최고속도·시간 범위로 충분하면 `dt_motion_rule[]`를 `dt_controller_set_rules()`에 주입한다. 설정된 규칙이 서로 다른 kind에 동시에 맞으면 이벤트를 거부한다. 규칙0개는 기본 휴리스틱으로 돌아간다는 점에 주의한다.
4. 스매시/발리/슬라이스/로브는 현재 `set_mode()`로 선택한 의도를 전달할 수 있다. 이는 IMU가 네 동작을 자동 식별했다는 증거가 아니다. 새 자동 분류를 만들면 별도 라벨·혼동 행렬로 검증한다.
5. 고유 스킬은 SWING.kind8로 같은 입력 경계를 사용한다. 실제 스킬 사용권과 재고3개 제한은 game이 판정한다. 펌웨어에서 재고를 만들거나 점수를 부여하지 않는다.

**출력 계약:** SWING.kind1FH/2BH/3서브/4스매시/5발리/6슬라이스/7로브/8스킬, power0..100, yaw±18000cdeg, pitch±9000cdeg. 새 이벤트는 새 eventId, 같은 이벤트의 재전송은 같은 eventId/원래 시각과 새 sequence를 쓴다. `packet()`과 공통 코덱을 사용하고 임의 바이트 배열을 만들지 않는다.

**완료 조건:** 세 동작별 정밀도/재현율·오검출/분·누락/중복·인식 지연 분포·환경/표본 수·실패 사례가 있다. 합성 테스트 통과를 실물 정확도로 표시하지 않는다. 제품 합격 목표치는 실제 담당자와 정한 후 결과와 나란히 기록한다.

## 60 PC·Unity에서 확인할 순서와 수정 범위

1. 센서용 NU40 펌웨어는 `DT_INPUT_MOTION=0`과 기본 가속도 피크 프로필을 사용한다. NU54 빌드에 자동 적용되는 `DT_BUTTON_PROFILE=1`은 tick의 센서 판정을 건너뛰므로 IMU 실기 경로로 사용하지 않는다. NU54로 센서를 추가 시험하려면 보드별 profile 선택을 먼저 명시적으로 수정하고 NU40 기본 경로를 보존한다.
2. 정상 본딩 연결·CCC 구독 후 LINK_REPLY.kind0을 확인한다. kind1은 센서 없는 버튼 모드다. 송수신 성공은 보정 성공과 다르다.
3. Unity 설정→입력 컨트롤러→무선→검색→연결→정지 보정. **현재 연결의 CALIBRATE 명령에 대한 성공 ACK + ready/calibrated flags**가 필요하다. SDK의 보정 gate를 끄지 않는다.
4. 설정 테스트에서 서브·FH/BH·스킬 세 범주를 확인한 뒤 저장한다. 설정에서 확인한 이벤트가 경기에 새어 나가면 실패다.
5. SDK는 power/100과 yaw,pitch 각각/4500 뒤[-1,1] clamp로 정규화한다. 게임은 그 입력에 CSV의 `shot.yaw`, `shot.pitch_aim` 등을 적용한다. IMU 상대각이 코트의 절대 방향과 같다고 해석하지 않는다. 체감 조준 변경은 이 변환과 game CSV를 분리해 조정한다.
6. 일반 경기에서 선택한 무선 source만 입력한다. Near/Far는 게임이 매핑한다. FH/BH가 수신됐어도 `WrongPhase`, `OutOfReach`, `Height` 등 타격 규칙 때문에 적중하지 않을 수 있다. 입력 수신과 실제 공 접촉을 따로 기록한다.

**다음 설계로 바꾸는 경우:** PC가 raw 센서값으로 분류하게 하려면 현재 SWING을 raw로 덮어쓰지 않는다. protocol에 진단/샘플 계약(축·단위·시각·ODR·순번·손실·프레이밍·버전)을 먼저 설계하고, firmware 송신/SDK 재조립·분류를 별도 모듈로 추가한다. v1/v2·20바이트 기존 액션 소비자는 계속 작동해야 한다. 지금 구현된 기능으로 raw 스트리밍이 가능하다고 안내하지 않는다.

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
| 보정 성공, SWING 없음 | gyro+accel release로 latch 해제, 회전 trigger/accel 국소피크/min duration/cooldown, 규칙 미일치 |
| SDK가 이벤트 거부 | Motion profile, 현재 보정 ACK, flags, CRC·중복·sequence·현재 연결 epoch |
| UI 테스트는 통과, 경기 공이 안 맞음 | 선택 source·게임 phase·거리/높이·현재 shot/rally ID·receipt time |
| FH/BH 뒤집힘 | 센서 장착 좌표·axis·left_handed. 게임 Near/Far를 장치에서 반전하지 않기 |

인수 PR에는 실제 배선표·장착 방향·소스/도구/이미지 해시, 진단 CSV 설명, 호스트 결과, 실제 동작별 혼동 행렬, 일반 Unity 설정/서브/타격/스킬 결과와 미검증 목록을 포함한다. 센서를 보유한 개발자가 수행하지 않은 결과는 빈칸/미검증으로 둔다. 코덱 변경이 있다면 protocol→firmware·SDK→game→문서 순서로 참조를 고정한다.

## 실험으로 선택한 임계값을 실물에 인수하기

[임계값 실험](threshold-experiment.md)의 `gesture-detector.csv`와 `gesture-rules.csv`는 분리한 합성 조정/선택/최종 평가의 결과입니다. 새 합성 분포693/700, 백핸드5·발리2 누락과 백핸드99→95 회귀를 포함합니다. 실물 담당자는 같은 C 실행기로 녹화한 사용자/세션별 자료를 분리해 검증하고 범위를 조정해야 합니다. 기본 앱의 빠른 peak 프로필·상황 BLE 연동 미구현 경계는 그대로입니다.
