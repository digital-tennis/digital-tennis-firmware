# Digital Tennis · 라켓 컨트롤러 펌웨어

외장 IMU 샘플을 보정·가공해 라켓 액션으로 판정하고, protocol 규격의 이벤트를 BLE로 PC에 전달한다.

전체 제품은 NU40-DK에 **별도 외장 IMU**를 부착하는 라켓 컨트롤러와 Unity PC 테니스 게임으로 구성한다. 최초 계획은 **12일·3명**이다.

## 주요 기능

- 100Hz 샘플링 경계, 보정·구간 모션·버튼 디바운스
- 유한 송신 큐·재전송/만료·연결 epoch 격리
- 암호화된 GATT와 영속 본딩, NU40-DK 및 센서 없는 NU-54 검증 타깃

## 빠른 시작

CMake/C99(호스트), NCS3.4.1·Zephyr4.4.2 및 Nordic 도구체인(보드). 외장 IMU 모델/배선은 미확정.

```sh
cmake -S . -B .work/host
cmake --build .work/host
ctest --test-dir .work/host --output-on-failure
```

## 책임과 현재 범위

센서 종류와 샘플 주기·장착 축·보정·물리 액션은 이 제품이 소유한다. 코트/점수/선수 ID는 보내지 않는다. NU-54는 임시 실기 타깃이며 NU40-DK+별도 외장 IMU 요구를 대체하지 않는다. 보드 컴파일과 IMU 정확도는 구분한다.

## 구조와 문서

`src/hw/driver/`: 센서/GPIO · `src/ap/`: 판정/송신 · `src/common/`: 포트/데이터 · `boards/`: 보드 · `tools/`: 컴파일/검토된 설치

[구조·GPIO·보안·승인된 설치](docs/architecture.md), [보드 빌드 근거](docs/board-build-validation.json), [송신/모션 검수](docs/controller-refinement-validation.json), [최신 보안/GPIO 빌드 근거](docs/bonding-build-validation.json)

## 도움말과 기여

재현 가능한 문제는 [이슈](https://github.com/digital-tennis/digital-tennis-firmware/issues)로 남긴다. 관리 주체는 [Digital Tennis](https://github.com/digital-tennis)이며 [기여 안내](CONTRIBUTING.md)를 따른다. 검증 커밋을 고정하고 실제 장치 성공·모의 검사·교차 빌드를 구분한다.

## 라이선스

프로젝트 자체 코드·문서는 **[Apache-2.0](LICENSE)**이다. [NOTICE](NOTICE)와 [외부 코드·자산 고지](THIRD_PARTY_NOTICES.md)를 함께 확인한다. 외부 자산에 Apache-2.0을 덮어쓰지 않는다.
