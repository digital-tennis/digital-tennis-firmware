# DT-HW-31-21-00 · Air Band 참고 IMU 처리 적용

## 00 문서 관리

R01 / 2026-10-20. NU40-DK + **별도 외장 6축 IMU**가 제품 대상이다. 최초 12일·3명·36PD 계획은 유지한다. 이 문서의 번호/절 구성은 자체 문서 관리 방식이며 ATA iSpec 2200 적합성을 주장하지 않는다. 실제 센서 배선·인식률은 후속 담당자가 검증한다. 이번 작업에서 flash, 추가 NU54 버튼 시험은 하지 않았다.

## 10 참고 자료와 분석 결과

- [hex-aragon/air-band 고정 소스](https://github.com/hex-aragon/air-band/tree/d89ce32a460b34c0cc740235dc9a76e74396a094), 분석 파일 `air-band/firmware/air_band_right/air_band_right.ino`, `air-band/web/src/ble.ts`, 내부 README.
- [시연 영상](https://www.youtube.com/watch?v=T8Lj7S-T7LY): 스틱 동작이 드럼 화면의 타격 표시로 이어지는 모습을 확인했다. 영상은 우리 테니스 센서 정확도나 지연 수치의 근거가 아니다.
- [제작자의 설명](https://next-ai-agent.tistory.com/m/8): 외장 센서를 스틱에 고정해 입력·화면 반응을 확인하는 개념 검증이며 TinyML은 후속 방향으로 설명한다.

분석한 소스는 Arduino/Bluefruit 기반이다. 100Hz 목표로 가속도 세 축을 읽고 20개 샘플을 보관하며, 국소 최대값을 다음 샘플로 확인한다. 임계값·불응기를 적용하고 세기를 계산해 별도 송신 태스크로 8바이트 NUS 이벤트를 보낸다. `classifyHit()`은 이 커밋에서 항상 0을 반환한다. 따라서 **TinyML이나 테니스 동작 분류가 구현된 코드를 가져온 것이 아니다**. 가속도만 읽는 참조 경로는 테니스의 방향 판정에 필요한 자이로 초기화·채널 취득을 대체할 수 없다.

해당 커밋에 라이선스 파일을 발견하지 못했다. 소스·주석·웹 자산을 복사하지 않았다. 일반적인 피크 검출 원리를 참조하여 기존 Digital Tennis C 모듈과 Zephyr 드라이버 위에 독립 구현했다. 원본에 없던 테니스 판정·테스트를 원본 프로젝트의 기능으로 표현하지 않는다.

## 20 적용한 구조와 책임

```text
선택한 외장 센서 overlay → Zephyr 드라이버(WHO_AM_I/단위 변환)
  → external_imu.c: accel m/s², gyro deg/s, 보드 시각
  → 10ms deadline 취득 / raw 20개 원형 버퍼
  → 정지 보정 / 축 변환 / 자이로 필터
  → 회전 구간 + 가속도 피크 확인 / 분류 / 세기
  → 기존 v2 20바이트 SWING → 기존 비동기 BLE 큐
  → SDK의 본딩·보정·중복 검사 → Unity 일반 입력
```

모든 액션 판정은 펌웨어에서 수행한다. protocol은 바이트·단위·범위를 정의하고 SDK는 검증/정규화하며, game은 서브/타격/스킬이 현재 경기에서 가능한지 판정한다. 새로운 메시지 필드가 없으므로 protocol·SDK·game 패키지 참조를 바꿀 필요가 없다. NUS 드럼 패킷을 게임에서 해석하지 않는다.

| 적용 지점 | 구현 |
| --- | --- |
| `src/main.c`, `src/common/dt_sample_clock.h` | 작업 시간에 10ms를 더해 누적 지연시키던 대기 대신 절대 deadline. 늦은 주기는 건너뛰고 따라잡기용 반복 fetch 금지 |
| `dt_motion_peak_defaults()` | 실제 앱의 기본 센서 판정 프로필. 기존 `dt_motion_defaults()`의 자이로 release 방식은 명시적으로 선택 가능 |
| `dt_controller_tick()` / `acceleration_tick()` | 회전 시작 후 가속도 상승→국소 피크→다음 샘플 확인. 기본 20ms 이상 회전 구간 필요 |
| `dt_controller_copy_window()` | 최근 최대20개 raw6축 샘플을 오래된 순으로 복사. capacity가 작으면 최신 부분집합. 100Hz 기준 약200ms이며 실제 공백은 시각으로 확인 |
| `dt_motion_config` | gyro/축 설정에 `acceleration_peak`, `trigger_g`, `release_g`, `max_power_g` 추가. init에서 유한값·순서·범위 검사 |
| `external_imu.c`, `imu_trace.c` | 선택적인 bounded UART 진단. 센서 루프는 `K_NO_WAIT`, 별도 낮은 우선순위 태스크가 출력 |
| `imu_replay`, `imu-trace-to-csv.py` | UART→명시 단위 CSV→동일 C 판정/프로토콜 인코딩. BLE나 보드를 열지 않는 호스트 도구 |

### 20.10 피크 판정과 한계

가속도 크기는 `sqrt(ax²+ay²+az²)/9.80665`로 g로 바꾸며 **중력 1g를 포함한다**. 기본 trigger=2.5g, release=1.5g, power 상한=12g다. power는 `[trigger_g,max_power_g]`를 `[1,100]`으로 변환하고 범위를 제한한다. 세기 기준은 종전 gyro 최대속도 대신 accel 피크다. 분류 규칙의 `min_peak/max_peak` 단위는 계속 **gyro deg/s**다.

회전 시작/해제 기본값은 220/80deg/s, filter_alpha=.25, 최소 회전20ms, 최대1000ms, cooldown200ms다. FH/BH는 매핑한 gyro Z 부호, 서브는 peak gyro Y 기준이라는 기존 휴리스틱을 사용한다. 장착 방향에 따라 틀릴 수 있으므로 실측 데이터로 수정한다. 위치 추적이나 학습 모델이라고 부르지 않는다. raw 윈도우는 향후 실측 분류기의 입력으로 활용할 수 있지만 TinyML 모델은 포함하지 않는다.

피크 확인에는 다음 유효 샘플 한 개가 필요하다. 100Hz 목표에서 약10ms의 확인 지연이며, 필터·취득·스케줄링·BLE·Unity를 포함한 전체 지연은 실제로 측정해야 한다. SWING 시각은 **확인 시각이 아니라 피크 샘플 시각**이다. 각도는 해당 피크까지의 상대 회전량이며 절대 heading/라켓 공간 위치가 아니다.

회전만 하거나 병진 가속도만 흔들리는 경우에는 타격을 만들지 않는다. 피크 전 최소 구간이 없는 한 샘플 잡음을 거부한다. 한 스윙은 한 이벤트만 만들고 gyro와 accel이 해제 조건 아래로 내려온 뒤 cooldown까지 지나야 새 이벤트가 가능하다. 중복 timestamp와 역행 timestamp는 무시하고 정상 uint32 wrap은 허용한다. 센서 오류/NaN/긴 공백은 보정과 스윙을 무효화하며 윈도우를 비운다. 특수샷/스킬은 기존 명시 모드·규칙 API를 사용하며 자동 인식을 주장하지 않는다.

## 30 외장 센서 선택과 배선 확인

아래는 **선택 가능한 컴파일 예제**다. 센서가 실제로 연결되었다는 기록이 아니다. 기본 빌드는 alias가 없어 계속 sensor-not-ready로 동작한다. 전압·리비전·실제 헤더·주소·장착 축은 [인수 표 §20](imu-developer-handoff.md#20-시작-전에-실물-담당자가-작성할-표)을 먼저 완성한다.

| overlay | 명시 모델/주소 | 취득 범위·ODR |
| --- | --- | --- |
| `overlays/nu40-mpu6050.overlay` | MPU6050, AD0=low 가정의0x68 | ±16g/±2000deg/s. 현 Zephyr 드라이버는 DLPF를 설정하지 않으므로 divider0 유지, hardware ODR≥1kHz. 앱은100Hz 목표로 취득 |
| `overlays/nu40-lsm6dso.overlay` | **LSM6DSO**, SA0=high 가정의0x6B | ±16g/±2000deg/s, accel·gyro 모두104Hz 활성화 |

LSM6DS3/LSM6DSL/LSM6DSO를 같은 compatible로 선언하지 않는다. 이 작업은 LSM6DS3/DSL 모든 모델의 호환성을 보장하지 않는다. 장치 이름만 비슷하다고 WHO_AM_I 검사를 우회하지 않는다. 기존 Zephyr 드라이버가 read/초기화 실패를 반환하면 정상 샘플을 만들지 않는다.

**핀 차이:** 현재 vendored NU40 Zephyr DTS의 i2c0는 SDA=P0.22, SCL=P0.21이다. 두 overlay는 이 정의를 사용한다. Air Band의 Arduino Wire는 SDA=P0.26/SCL=P0.27이며 이 저장소에서는 그 핀이 SPI1/SD 경로에 쓰인다. Arduino의 핀/버튼 번호를 그대로 복사하지 않는다. Air Band 배선을 재사용하려면 실물 회로를 대조한 별도 pinctrl overlay와 SPI 충돌 해소를 검토한다. 참고 README의0x6A 충전 IC 추정도 모든 NU40 리비전의 확정 사실로 사용하지 않는다.

두 예제는 센서 폴링이다. 실제 data-ready IRQ나 하드웨어 timestamp가 보장되는 경로는 아니므로 후속 담당자가 샘플 누락/지터·포화·새 데이터 여부를 측정한다. 104Hz 센서와100Hz 앱의 비동기 특성도 측정 대상이다. 드라이버/버스 취득 중 블로킹 시간은 있을 수 있지만 BLE 송신과 진단 출력 대기로 샘플링을 막지는 않는다.

`nu40-imu-i2c.dtsi`는 vendored DTS를 고치지 않고 센서 앱의 pinctrl만 보완한다. 원래 i2c0 default 그룹의 `low-power-enable`을 제거하고 sleep에는 SDA/SCL 양쪽을 지정한다. Nordic pinctrl 드라이버는 low-power 입력 버퍼를 disconnect하므로 기본 취득과 절전 구성을 구분해야 한다. 이 수정의 컴파일 확인은 실제 핀 전기 신호 검증과 다르다. 적절한 외부 I2C pull-up은 실제 모듈 회로에서 확인한다.

## 40 실행·기록·재생 과제

호스트 검사(보드 없음):

```sh
cmake -S . -B .work/host -DDT_PROTOCOL_ROOT=/absolute/path/digital-tennis-protocol
cmake --build .work/host
ctest --test-dir .work/host --output-on-failure
```

센서 담당자는 **검토한 overlay 한 개**로 컴파일한다. 아래는 LSM6DSO 예제이며 설치 명령이 아니다. 기존 Nordic SDK/도구체인 경로를 사용한다.

```sh
python3 tools/build-board.py --sdk-root /absolute/ncs/v3.4.1 \
  --toolchain /absolute/nordic/toolchain \
  --protocol /absolute/path/digital-tennis-protocol \
  --overlay overlays/nu40-lsm6dso.overlay --out .work/nu40-lsm --trace
```

`--trace`는 일반 빌드에서 꺼져 있다. 개발 UART 출력 `DTIMU,time_ms,ax_mmps2,ay_mmps2,az_mmps2,gx_mdps,gy_mdps,gz_mdps`를 담당자가 로그 파일로 저장한다. 실제 콘솔은 현재 board의 UART0 설정이며 USB CDC가 자동으로 이 로그를 내보낸다고 가정하지 않는다. 32행 큐가 차면 진단 행을 버리고 원 시각 공백을 남긴다. 개인정보/주소/본딩 키는 기록하지 않는다. 기록기가 게임 BLE raw 스트리밍을 추가하는 것은 아니다.

NCS 3.4.1의 기존 생성 폴더를 재설정할 때 `extra_kconfig_options.conf`의 TLS config 경로 인용부호 경고로 실패하는 경우가 있었다. 같은 명령에 `--pristine`을 추가해 **해당 생성 빌드 폴더만** 다시 만들면 된다. 소스/SDK를 지우거나 보드에 쓰는 작업이 아니다. 최종 두 센서 빌드는 이 방식으로 확인했다.

```sh
python3 tools/imu-trace-to-csv.py uart.log samples.csv
.work/host/imu_replay samples.csv > events.csv
```

변환기는 기존 출력 파일을 덮어쓰지 않는다. 다른 부팅 로그는 무시하되 깨진 DTIMU 행은 실패한다. CSV 열은 `time_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps`다. 재생기는 첫 유효 샘플 후 실제 CALIBRATE 명령을 넣으므로 **처음65개 이상을 연속 정지**로 기록한다. 이후 정상 취득·표준 프로필로 분류해 시간/kind/power/상대각/20바이트hex를 출력한다. 부팅이 섞인 여러 시퀀스는 먼저 세션별로 분리한다. 재생 시각 공백을 보간하거나 이미 보정된 상태로 강제 변경하지 않는다.

실측 데이터에서 `dt_motion_peak_defaults()`의 설정을 시작점으로 장착축·필터·trigger/release/cooldown·power 범위를 조정한다. 설정은 현재 펌웨어 빌드 시 주입하며 런타임 CSV/원격 설정 기능은 아니다. game의 CSV 밸런스는 수신한 정규화 입력 이후의 경기 효과를 담당한다.

**후속 실물 완료 기준:** 동작별 정밀도/재현율·가만히 들기/걷기 오검출·반동 중복·누락·지터/지연 분포·포화·분리/재보정과 Unity 연결→보정→테스트→저장→경기 결과를 기록한다. 튜닝용과 평가용 데이터를 분리한다.

## 50 검증 근거

[기계 판독 검증 기록](imu-peak-validation.json)을 확인한다. C 호스트 검사와 실제 드라이버를 포함한 보드 컴파일은 실제 센서·전파·사람의 동작 인식률 검증을 대체하지 않는다. NU54 센서 없는 버튼 모드의 빌드도 회귀 대상이다. 임계값 통과를 증명하기 위한 합성 CSV를 사람이 움직인 측정 자료로 표시하지 않는다.
