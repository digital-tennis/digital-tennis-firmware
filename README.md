# Digital Tennis 외장 IMU 펌웨어

NU40-DK에 별도로 부착한 IMU의 데이터 획득, 보정, 스윙 인식, BLE 전송를 담당하는 독립 제품 저장소입니다. 전체 프로젝트는 **외장 IMU를 별도로 부착한 NU40-DK**, Unity PC 게임, 1인 AI와 온라인 2인 대전을 12일·3명으로 개발합니다.

## 현재 상태

[참고 프로젝트 기반 구조와 재현 안내](docs/architecture.md)를 구현했습니다. 외장 IMU 포트→보정·필터·기본 스윙→공통 프로토콜→BLE Peripheral로 책임을 분리합니다. `cmake -S . -B .work/build`, `cmake --build .work/build`, `ctest --test-dir .work/build --output-on-failure`로 실물 없이 호스트 검사를 실행합니다.

호스트 검사 통과와 실제 보드 성공은 다릅니다. 외장 센서 모델·배선은 아직 미확정입니다. 로컬 NCS3.4.1/Zephyr4.4.2에서 보드용 ELF/HEX 교차 빌드를 검사하며, flash·실제 BLE/IMU 인식은 유예합니다. 기본 임계값은 실기 정확도가 보장된 값이 아닙니다. 참조 원본의 Central 역할을 그대로 복사하지 않고 라켓용 Peripheral 경계를 구성했습니다.

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

실제 구조는 `boards/ · overlays/ · src/common/ · src/hw/driver/ · src/ap/ · tests/ · tools/`입니다. 호스트 검사와 보드 대상 컴파일은 독립 경로입니다. `tools/build-board.py`는 설치된 SDK/툴체인을 명시적으로 받고 컴파일만 수행하며 플래시 기능은 제공하지 않습니다.

## 검증과 협업

- [12일 WBS](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/planning/wbs.md)
- [컨트롤러 계약](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/contracts/controller-protocol.md)
- [온라인 계약](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/contracts/online-protocol.md)
- [수용 테스트](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/testing/acceptance.md)
- [브랜치·PR 작업 방법](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/guides/contributing.md)
- [모든 제품과 의존 관계](https://github.com/digital-tennis/digital-tennis-docs/blob/main/docs/project/repositories.md)

라이선스는 코드와 제3자 자산 조건을 검토한 뒤 팀이 선택합니다.

## 송신·버튼·모션 및 센서 없는 통신 확인

32개 송신 큐와 기한/재전송,30ms 버튼 튐 제거, 스윙 시작→최고속도→release와 상대각/세기, 최대8개 녹화 프로필 규칙을 제공합니다. 기본 자동 분류는FH/BH/서브이며 특수샷은 선택 모드나 검증된 프로필을 사용합니다. LINK_PROBE/REPLY는 센서 없이 연결·송수신을 확인하며 보정·게임 준비를 만들지 않습니다. 실제 배선/GPIO/사람 인식률은 유예합니다. [구조와 연결 절차](docs/architecture.md), [이번 호스트·보드3구성 컴파일 기록](docs/controller-refinement-validation.json)을 확인하세요. 합성 overlay는 실제 보드에 올리지 않습니다.
