# 외장 IMU 라켓 펌웨어 구조

원래12일·3명 계획과 **NU40-DK에 별도 부착하는 외장 IMU** 조건을 유지한다. `chcbaram/nu40dk-kit`의 고정 커밋 `df4367ed03d35c8599dc6007f049b2f4df9812b3`을 검토했다. 원본의 `src/ap`, `src/hw/driver`, 보드 정의와 Zephyr 구성을 참고했고 보드 파일만 원문 MIT 고지와 함께 보관했다. 참조 Xbox 예제는 BLE Central로 컨트롤러를 받는 코드다. 본 앱은 **Peripheral/GATT Server**로 PC에 라켓 의도를 보낸다. 원본의 시작 시 전체 페어링 삭제·추측 CCC 핸들·무조건적 짧은 데이터 읽기는 복사하지 않았다.

```text
boards/baram/nu40dk_kit    참조 보드 정의(실물 리비전과 일치 여부 미확인)
overlays/                사용자가 확인할 별도 센서·배선 설정
src/hw/driver/           외장 IMU 포트: Zephyr sample_fetch/accel/gyro
src/common/              SI 가속도/deg/s 샘플, 설정, 포트 계약
src/ap/dt_controller.c   정지 영점·스윙 구간·상대각·선택/규칙 분류·명령 상태
src/ap/ble_peripheral.c  서비스/구독/암호화된 Write/Notify·bond 저장
src/ap/dt_tx_queue.c     32개 큐·기한·재전송·연결 세대
src/ap/dt_button.c       30ms debounce·부팅 때 누른 버튼 제외
src/main.c               100Hz 취득/버튼·명령 처리와 독립 TX worker
protocol pinned SHA      공통20바이트 코덱
tests/                   실물 없는 가짜 센서·실패 조건 검사
```

센서 장착은 확인된 overlay의 `external-imu` alias로만 연결한다. alias가 없거나 장치 미준비·샘플 오류이면 ready/calibrated를 거짓으로 두고 스윙을 만들지 않는다. 내장 센서로 대체하지 않는다. Zephyr 센서 드라이버의 가속도는m/s², 각속도rad/s는deg/s로 변환한다. 장착 행렬·왼손 처리·필터·임계값은 `dt_motion_config` 주입값이다. 기본값은 **초기 소프트웨어 시험값**이고 실물 인식률 근거가 아니다.

CALIBRATE는 정지 상태64개 샘플로 gyro bias를 구한다. 움직이면 연속 표본을 다시 모으며5초에 실패한다. 진행 중에는BUSY, 완료된 명령ID는최근64개 결과를 재사용한다. 센서 실패·시간 불연속·끊김은 보정을 무효화한다. 앱의 `dt_motion_peak_defaults()`는 필터된 각속도 구간 안에서 가속도 국소피크를 다음 샘플로 확인한다. gyro/accel 해제·cooldown으로 한 스윙의 중복 이벤트를 막는다. 이전 각속도 release 방식은 `dt_motion_defaults()`로 비교할 수 있다. 최소20ms·최대1000ms 구간과100ms 샘플 공백 기준은 주입 설정으로 조절한다. 피크까지의 yaw/pitch 상대 회전량과 가속도 피크 기반 power를 전송한다. 비교 release 프로필만 최고 각속도 기반 power를 사용하며 절대 위치/전역 방향을 추정하지 않는다. 기본 자동 분류는 FH/BH/서브다. 특수샷은 명시적 모드 선택으로 구분하고, `dt_controller_set_rules`는 각도/최고속도/시간 범위의 최대8개 녹화 기반 프로필을 주입한다. 프로필의 미일치/모호한 겹침은 입력을 만들지 않는다. 테스트의8개 프로필은 합성 입력이며 실제 사람 제스처 정확도 주장이 아니다. 실제 장착 데이터로 프로필과 임계값을 정해야 한다.

암호화된CCC 구독과명령Write, 보정 완료 상태를 사용한다. GATT write 성공과 보정 ACK 성공은 다르다. 연결 종료 시 보정과 명령 이력을 폐기하고 광고를 재시작한다. 참고 NU-54의 연결 ref/실제 구독 확인/순차 송신/BT settings 패턴을 적용했다. NU40는 NVS, 임시 NU54는 분리된 ZMS의 BT bond를 부팅 때 로드하며 자동 초기화하지 않는다. 기존 참고 보드의 펌웨어와 테니스 펌웨어는 별도 제품이다. 배터리 측정은 구현하지 않아255(미측정)를 보낸다.

## 장비 없이 실행

```sh
cmake -S . -B .work/build -DCMAKE_C_FLAGS='-fsanitize=address,undefined'
cmake --build .work/build
ctest --test-dir .work/build --output-on-failure
```

프로토콜은 검토한 Git 커밋을 FetchContent로 고정한다. 오프라인 개발 시 `-DDT_PROTOCOL_ROOT=/absolute/checked/protocol`로 동일 소스를 지정한다. 가짜 센서 미준비→보정→ACK/중복명령→FH/BH→히스테리시스→8명시의도→잘못된샘플을 검사한다.

## 보드 통합 시 수행할 것

검증 환경은 설치된 NCS3.4.1(LTS), Zephyr4.4.2, GNU14.3.0/Zephyr SDK1.0.1이다. 소스 커밋·컴파일러/도구 버전·ELF 해시·메모리 사용량은 `docs/board-build-validation.json`에 기록한다. 현재 SDK 스키마에 필요한 `board.yml.full_name`만 참고 보드에 추가했다. BLE 광고는 현재 API와 파일 수명의 payload를 사용하고 이름을 scan response에 분리해 광고 크기 한계를 지킨다.

```sh
python3 tools/build-board.py --sdk-root /absolute/ncs/v3.4.1 --toolchain /absolute/nordic/toolchain --protocol /absolute/checked/protocol
python3 tools/build-board.py --sdk-root /absolute/ncs/v3.4.1 --toolchain /absolute/nordic/toolchain --protocol /absolute/checked/protocol --overlay tests/compile-fixtures/mpu6050.overlay --out .work/zephyr-external-compile
```

첫 빌드는 외장 alias가 없는 상태를 컴파일한다. 두 번째는 **컴파일 전용 합성 MPU6050 overlay**로 실제 Zephyr6축 API를 호출하는 드라이버 분기까지 컴파일한다. 모델/주소는 테스트 가정이며 제품의 센서/배선 결정이 아니다. 두 경로 모두 실물 연결·flash를 하지 않는다. 테스트 fixture를 플래시하지 않는다.

실제 작업에는 검토한 overlay를 `--overlay`에 전달한다. EXTRA_DTC_OVERLAY_FILE로 추가하여 기본 보드의 GPIO/저장 overlay를 보존한다. 외장 IMU 모델·전압·헤더·주소·지원 드라이버·장착 행렬과 실제 보드 리비전은 여전히 검토가 필요하다. 보드 대상 빌드 성공은 실제 부팅·전파·OS페어링·센서 정확도 성공과 다르다. SDK 변경으로 캐시가 맞지 않으면 생성 빌드 폴더만 `--pristine`으로 재생성한다. Kconfig 경고를 무시하도록 바꾸지 않는다.

## 센서 없는 BLE 확인·송신 안정화

LINK_PROBE(130)는 센서 준비/보정과 독립적으로 LINK_REPLY(131)를 돌려준다. 실제 ready/calibrated flags는 그대로이며 보정·스윙·설정 저장을 만들지 않는다. BLE Write callback은 검증한 명령을8개 큐에 넣고 즉시 반환한다. 센서 루프와 별도의 송신 worker는32개 큐에서 제어/스윙/버튼을3회 전송하며, 같은 eventId/원래 deviceTimeMs와 매 전송 새 sequence를 사용한다. STATUS는1회다. 일시적인 송신 실패는20ms 뒤 재시도하고 입력200ms/제어1000ms 기한을 넘기면 폐기한다. 가득 찬 큐의 가장 오래된 항목을 버리고 drop 수를 보존한다. 수신/송신 작업의 연결 세대를 검사하고 구독 변경·단절 시 큐를 비운다.

## 버튼과 샷 선택

`racket-confirm/start/pause/skill/shot` alias를 실제로 검토한 overlay에 지정한다. alias가 없으면 GPIO를 읽지 않는다.30ms 안정 상태 뒤 눌림1회만 확정하고 부팅 때 이미 눌린 버튼은 놓을 때까지 무시한다. confirm/start/pause는 BUTTON1/2/3, skill은 준비·보정 후 SWING8이며 shot은 자동→스매시→발리→슬라이스→로브→자동을 순환한다. 실제 스윙은 선택 모드의 kind와 측정된 상대각·세기를 사용한다. NU40 GPIO/active-low는 원본 button.c의 _DEF_LOW와 보드 DTS를 근거로 앱 overlay에 반영했다. 논리 눌림/실제 리비전은 실물 확인이 남아 있다. 배터리255는 계속 미측정 값이다.

참고 제품의 확인 커밋은 `ee12b7519336ed276060b83c69f4f863f557443a`이며 `p01-device-firmware/app/src/ble_links.c`와 `p05-operations-backoffice/internal/ble/central.go`의 연결 패턴만 참고했다. 지갑/결제/프로비저닝 데이터나 기능은 본 제품에 포함하지 않는다.


## DT-HW-31-10-00 · 적용 보드와 보안

문서 R02, 2026-10-18, PR 검토본. 실제 제품은 **NU40-DK + 별도 외장 IMU**다. NU-54 시험 성공을 NU40 실기나 IMU 정확도로 표시하지 않는다.

| 대상 | 역할/SDK qualifier | 버튼 매핑 | 저장 |
| --- | --- | --- | --- |
| NU40-DK | 제품 기본, nu40dk_kit/nrf52840 | confirm P0.31, start P1.7, pause P1.6, skill P1.5 | 기본 보드 NVS |
| NU-54 | 현재 연결된 센서 없는 임시 시험 보드, nu54v_dk/nrf54l15/cpuapp | vendored button0..3의 DT alias | Tennis 전용 ZMS0x168000..0x170000, 별도 영속 identity1 |

NU40 보드/극성은 [nu40dk-kit 고정 원본](https://github.com/chcbaram/nu40dk-kit/tree/df4367ed03d35c8599dc6007f049b2f4df9812b3/firmware/10_xbox/nu40dk-kit-xbox) DTS와 src/hw/driver/button.c의 _DEF_LOW를 따랐다. vendored DTS를 덮어쓰지 않고 애플리케이션 overlay에 GPIO_PULL_UP|GPIO_ACTIVE_LOW를 명시했다. 외장 IMU 핀은 아직 추정하지 않는다.

현재 화면 없는 라켓은 **LE Secure Connections + Just Works 암호화/본딩**을 사용한다. PIN/숫자 대조에 의한 최초 페어링 MITM 인증은 없다. 사용자가 NU54 Tennis 시험 기기에 명시적으로 승인했다. CONFIG_BT_SMP_SC_PAIR_ONLY와 BT_BONDABLE을 사용하며 authenticated Level4를 강제하는 SC_ONLY와 혼동하지 않는다. CCC와 보안 읽기는 암호화가 필요하며 명령·Notify는 현재 peer의 level>=2와 bond 저장을 추가 확인한다. 보호된4바이트 특성은 [protocol ICD](https://github.com/digital-tennis/digital-tennis-protocol/blob/12dae689fd9b25187426829c606529508c100e65/spec/controller-v2.md)를 따른다. 키·PIN·주소를 값에 넣지 않는다.

NU54 overlay는 현재 직접 실행 시험용이다. 원래 slot1의 사용하지 않는 꼬리32KiB를 Tennis 설정으로 사용하므로 MCUboot/OTA와 병용하지 않는다. 최초 설치 전 빈 영역인지 확인하며, 기존 지갑 저장0x174000..0x17d000은 읽거나 지우지 않는다. NU40에 이 저장 맵을 적용하지 않는다. NU54는 별도 영속 identity1을 만들어 원래 앱의 OS GATT 캐시·bond를 재사용하지 않는다. 기본 제품 NU40는 기본 identity0이다. 출처·원래 MIT 고지는 boards/nucode/nu54v_dk에 보존한다.

### NU54 승인된 코드 교체

`tools/install-nu54-preview.py`는 기본적으로 계획만 출력한다. 실제 쓰기는 별도 승인 후 --execute, 검토한 HEX SHA256, 새로운 비공개 --backup 디렉터리를 모두 지정해야 한다.0..0x30000의 코드만 허용하고 UICR/전체 erase/자동 unlock을 금지한다. 최초 설치는 Tennis 저장이 비어 있어야 한다. 이미 설치한 Tennis를 수정할 때는 --previous-image와 성공한 --previous-receipt를 함께 주고 기존 코드 바이트가 일치해야 한다. 코드 섹터만 백업/교체하고128바이트 블록으로 공개 HEX 코드 구간을 최종 대조한다. 지갑·bond 데이터는 공개하지 않는다. 백업·HEX는 Git에 넣지 않는다. 실물 리비전/다른 앱에 이 도구를 일반 플래셔처럼 사용하지 않는다.

## 2026-10-19 센서 없는 실물 연결 결과

Mac 일반 Unity 창의 clean 빌드에서 NU-54 실제 본딩·암호화·LINK_PROBE/REPLY와 보드 reset 후 본딩 재사용을 통과했다. 보안 진단은 L2/bonded/SC를 확인했고 센서 ready/calibrated는 false를 유지했다. 센서 없는 보정 요청은 실패하고 게임의 동작 테스트·저장은 차단됐다. reset은 저장영역 읽기·삭제·flash 없이 수행했다. [현재 기계 판독 결과](bonding-build-validation.json)를 확인한다.

이 결과는 NU-54의 센서 없는 BLE 경계이며 NU40 실물 GPIO/BLE·외장 IMU 정확도·물리 Windows 검증을 대신하지 않는다. 최초 OS PIN/숫자 대조 팝업 조작을 확인한 것은 아니다. 이전 ATT15/SMP timeout은 당시 실패 기록으로 유지한다.

## NU-54 버튼 전용 프로필

NU54 보드 빌드만 DT_BUTTON_PROFILE을 명시합니다. vendored DTS의 SW1=P1.13, SW2=P1.09, SW3=P1.08, SW4=P0.04(active-low)를 앱 alias로 사용합니다. 순서대로 BUTTON4/5/6/7이며 메인 루프가 IMU 판정을 수행하지 않습니다. LINK_REPLY.kind=1이 현재 연결의 버튼 모드를 확인합니다. 센서 flags는0, power/각도는0이고 IMU 보정 요청은 실패 ACK입니다. NU40는 기존 alias·IMU 판정을 유지합니다. 키·설정 저장영역과 본딩 보안 정책은 변경하지 않습니다.

설치 도구는 debug USB 단일 패킷·128바이트 읽기를 사용합니다. 실행 중 CPU와 대용량 스마트 비교 읽기에서 관찰한 timeout을 피하려고 CPU를 정지한 뒤 기존 코드만 확인하고, 백업의 섹터 여백을 보존한 완전한 코드 섹터 파일을 기록합니다. 최종 대조는 공개 HEX에 명시된 주소만 수행합니다. 오프라인 updater 검사는 기존 바이트 보존·오래된 이미지/영역 거부·자동 unlock 금지를 확인하며 실물 검증과 구분합니다.

## IMU 피크 처리·개발 진단

10ms deadline 기반 취득, 최근20개6축 원형 버퍼, MPU6050/LSM6DSO의 명시적 overlay와 선택적 UART→CSV 재생 도구는 [Air Band 참고 적용 문서](air-band-imu-adaptation.md)에 정리했다. 기본 빌드는 외장 IMU alias가 없으므로 모션을 만들지 않는다. 예제 overlay의 컴파일 성공은 실제 배선·샘플·사람 동작 인식 성공과 다르다.

## 실험용 7종 모션과 가상 센서

`dt_motion_gesture_defaults()`는 최근128개 raw 샘플의 방향·회전 범위·시간·일관성·가속도 특징을 CSV 규칙으로 비교합니다. 기본 `main.c`는 빠른 가속도 피크 프로필/논리20샘플을 유지하며, 자료형 버퍼의 정적 할당은128개입니다. 서버나 Unity에서 raw값을 모으는 구조로 바꾸지 않았습니다. 별도의 경기 상태가 없는 overhead는 서브/스매시를 구분하지 않습니다. 경기 상태 BLE 명령은 미구현입니다. [가상 궤적·C 재생·실물 인수 문서](tennis-gesture-simulator.md)에서 재현 방법·지연과 역할 경계를 확인합니다.
