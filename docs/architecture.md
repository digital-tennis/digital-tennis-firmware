# 외장 IMU 라켓 펌웨어 구조

원래12일·3명 계획과 **NU40-DK에 별도 부착하는 외장 IMU** 조건을 유지한다. `chcbaram/nu40dk-kit`의 고정 커밋 `df4367ed03d35c8599dc6007f049b2f4df9812b3`을 검토했다. 원본의 `src/ap`, `src/hw/driver`, 보드 정의와 Zephyr 구성을 참고했고 보드 파일만 원문 MIT 고지와 함께 보관했다. 참조 Xbox 예제는 BLE Central로 컨트롤러를 받는 코드다. 본 앱은 **Peripheral/GATT Server**로 PC에 라켓 의도를 보낸다. 원본의 시작 시 전체 페어링 삭제·추측 CCC 핸들·무조건적 짧은 데이터 읽기는 복사하지 않았다.

```text
boards/baram/nu40dk_kit    참조 보드 정의(실물 리비전과 일치 여부 미확인)
overlays/                사용자가 확인할 별도 센서·배선 설정
src/hw/driver/           외장 IMU 포트: Zephyr sample_fetch/accel/gyro
src/common/              SI 가속도/deg/s 샘플, 설정, 포트 계약
src/ap/dt_controller.c   정지 영점·필터·기본스윙·명령 상태
src/ap/ble_peripheral.c  서비스/구독/암호화된 Write/Notify
src/main.c               100Hz 취득과 조립(단일 앱 루프)
protocol pinned SHA      공통20바이트 코덱
tests/                   실물 없는 가짜 센서·실패 조건 검사
```

센서 장착은 확인된 overlay의 `external-imu` alias로만 연결한다. alias가 없거나 장치 미준비·샘플 오류이면 ready/calibrated를 거짓으로 두고 스윙을 만들지 않는다. 내장 센서로 대체하지 않는다. Zephyr 센서 드라이버의 가속도는m/s², 각속도rad/s는deg/s로 변환한다. 장착 행렬·왼손 처리·필터·임계값은 `dt_motion_config` 주입값이다. 기본값은 **초기 소프트웨어 시험값**이고 실물 인식률 근거가 아니다.

CALIBRATE는 정지 상태64개 샘플로 gyro bias를 구한다. 움직이면 연속 표본을 다시 모으며5초에 실패한다. 진행 중에는BUSY, 완료된 명령ID는최근64개 결과를 재사용한다. 센서 실패·시간 불연속·끊김은 보정을 무효화한다. 필터된 각속도의 히스테리시스·200ms 초기 cooldown으로 한 스윙의 반복 검출을 막는다. FH/BH/위쪽서브는 최소 분류기이며 방향 적분·정밀 자세 추적은 구현하지 않아 yaw/pitch는0으로 보낸다. v2의 추가 샷/스킬은 상위 버튼·제스처 정책이 `dt_controller_intent`에 명시적으로 요청하는 경계다. 실제8제스처 인식 성공을 의미하지 않는다.

암호화된CCC 구독과명령Write, 보정 완료 상태를 사용한다. GATT write 성공과 보정 ACK 성공은 다르다. 연결 종료 시 보정과 명령 이력을 폐기하고 광고를 재시작한다. 영구 bond 저장 정책은 실제 제품 플래시/보안 요구와 함께 확정해야 하며 현재 RAM 연결만 다룬다. 배터리 측정은 구현하지 않아255(미측정)를 보낸다.

## 장비 없이 실행

```sh
cmake -S . -B .work/build -DCMAKE_C_FLAGS='-fsanitize=address,undefined'
cmake --build .work/build
ctest --test-dir .work/build --output-on-failure
```

프로토콜은 검토한 Git 커밋을 FetchContent로 고정한다. 오프라인 개발 시 `-DDT_PROTOCOL_ROOT=/absolute/checked/protocol`로 동일 소스를 지정한다. 가짜 센서 미준비→보정→ACK/중복명령→FH/BH→히스테리시스→8명시의도→잘못된샘플을 검사한다.

## 보드 통합 시 수행할 것

팀의 Zephyr/NCS·툴체인 버전을 확정하고 west 환경에서 `west build -b nu40dk_kit/nrf52840 . -- -DDT_ZEPHYR_APP=ON -DDTC_OVERLAY_FILE=/absolute/reviewed/external.overlay`를 실행한다. 실제 외장 IMU 모델·전압·헤더·주소·지원 드라이버·장착 행렬을 검토해야 한다. 현재 결과는 **호스트 C 검사 통과**이며 Zephyr/NCS 보드 빌드·flash·실제 전파·OS페어링·센서 정확도는 확인하지 않았다. SDK가 확정되기 전 보드 빌드 성공으로 보고하지 않는다.
