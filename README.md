# Digital Tennis 외장 IMU 펌웨어

NU40-DK에 별도로 부착한 IMU의 데이터 획득, 보정, 스윙 인식, BLE 전송를 담당하는 독립 제품 저장소입니다. 전체 프로젝트는 **외장 IMU를 별도로 부착한 NU40-DK**, Unity PC 게임, 1인 AI와 온라인 2인 대전을 12일·3명으로 개발합니다.

## 현재 상태

[참고 프로젝트 기반 구조와 재현 안내](docs/architecture.md)를 구현했습니다. 외장 IMU 포트→보정·필터·기본 스윙→공통 프로토콜→BLE Peripheral로 책임을 분리합니다. `cmake -S . -B .work/build`, `cmake --build .work/build`, `ctest --test-dir .work/build --output-on-failure`로 실물 없이 호스트 검사를 실행합니다.

호스트 검사 통과와 실제 보드 성공은 다릅니다. 외장 센서 모델·배선·SDK는 아직 미확정이며, 보드 빌드/flash·실제 BLE/IMU 인식은 유예 상태입니다. 기본 임계값은 실기 정확도가 보장된 값이 아닙니다. 참조 원본의 Central 역할을 그대로 복사하지 않고 라켓용 Peripheral 경계를 구성했습니다.

## 개발할 기능과 제출 기준

[초보 개발자용 상세 과제](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md)에서 상황·입력·출력·예제·예외·완료 기준을 확인합니다.

- [FW-01 외장 IMU를 연결하고 배선을 설명하기](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md#fw-01)
- [FW-02 외장 IMU 드라이버로 6축 샘플 읽기](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md#fw-02)
- [FW-03 라켓 좌표계와 정지 영점 보정](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md#fw-03)
- [FW-04 필터와 스윙 이벤트 인식기](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md#fw-04)
- [FW-05 BLE GATT와 보정 명령 전송](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md#fw-05)
- [FW-06 버튼과 오류 상태 및 실물 장착 검증](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/products/digital-tennis-firmware.md#fw-06)

## 개발 환경과 예상 구조

초기 담당 역할은 A입니다. 실제 IMU·배선·펌웨어 SDK·Unity·Windows·런타임 버전은 D1에 [환경표](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/project/overview.md)와 결정 기록에 남깁니다.

예상 폴더는 `src/drivers/ · src/motion/ · src/ble/ · tests/ · hardware/`입니다. 아직 존재하지 않는 폴더와 실행 명령을 구현 완료로 해석하지 않습니다. 채택 도구와 실제 구현에 맞게 갱신합니다.

## 검증과 협업

- [12일 WBS](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/planning/wbs.md)
- [컨트롤러 계약](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/contracts/controller-protocol.md)
- [온라인 계약](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/contracts/online-protocol.md)
- [수용 테스트](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/testing/acceptance.md)
- [브랜치·PR 작업 방법](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/guides/contributing.md)
- [모든 제품과 의존 관계](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/project/repositories.md)

라이선스는 코드와 제3자 자산 조건을 검토한 뒤 팀이 선택합니다.
