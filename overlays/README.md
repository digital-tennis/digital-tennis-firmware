# 실제 센서 overlay를 추가하는 위치

먼저 [IMU 개발 인수 가이드](../docs/imu-developer-handoff.md)의 §20 배선표를 작성합니다. 담당자가 확인한 센서 모델·버스·핀·주소로 새 `.overlay`를 만들고 `external-imu` alias를 지정합니다. 이 폴더에 검증된 센서 핀맵은 아직 없습니다.

`tools/build-board.py --overlay overlays/<실제로-검토한-파일>.overlay`로 기본 보드 overlay에 추가합니다. `tests/compile-fixtures/mpu6050.overlay`는 합성 컴파일 fixture이며 실제 배선/설치용 파일이 아닙니다. 빌드 성공, 실제 부팅, 센서 샘플 정상, 모션 인식률을 서로 다른 결과로 기록합니다.
