# 실제 센서 overlay를 추가하는 위치

먼저 [IMU 개발 인수 가이드](../docs/imu-developer-handoff.md)의 §20 배선표를 작성합니다. 담당자가 확인한 센서 모델·버스·핀·주소로 `.overlay`를 검토하고 `external-imu` alias를 지정합니다. 이 폴더에 **실물 검증된** 센서 핀맵은 아직 없습니다.

`nu40-mpu6050.overlay`와 `nu40-lsm6dso.overlay`는 명시적으로 선택하는 컴파일 예제입니다. 현재 Zephyr i2c0(SDA=P0.22/SCL=P0.21)를 사용하며 Arduino Air Band의 P0.26/P0.27 배선과 다릅니다. 주소는 각각0x68/0x6B입니다. LSM6DS3/DSL을 LSM6DSO로 취급하지 않습니다. [모델·범위·ODR·배선 차이와 실행 방법](../docs/air-band-imu-adaptation.md#30-외장-센서-선택과-배선-확인)을 먼저 읽으세요.

`tools/build-board.py --overlay overlays/<실제로-검토한-파일>.overlay`로 기본 보드 overlay에 추가합니다. `tests/compile-fixtures/mpu6050.overlay`는 합성 컴파일 fixture이며 실제 배선/설치용 파일이 아닙니다. 빌드 성공, 실제 부팅, 센서 샘플 정상, 모션 인식률을 서로 다른 결과로 기록합니다.
