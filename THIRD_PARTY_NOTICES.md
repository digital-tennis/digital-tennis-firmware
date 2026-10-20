# 외부 코드·자산 고지

프로젝트 자체 코드/문서의 Apache-2.0과 별도로 아래 원래 조건을 유지한다.

- NU40-DK board: [원본 MIT](third-party/nu40dk-kit-MIT.txt), [고정 출처](third-party/nu40dk-kit.json).
- NU54-DK board: [원본 MIT](boards/nucode/nu54v_dk/LICENSE), [고정 출처](boards/nucode/nu54v_dk/VENDORED.md).
- Zephyr/NCS/HAL/crypto는 사용한 SDK의 원래 고지를 유지한다. 소스에 남은 copyright/SPDX를 삭제하지 않는다.

## IMU 설계 참고

[hex-aragon/air-band](https://github.com/hex-aragon/air-band/tree/d89ce32a460b34c0cc740235dc9a76e74396a094)의 공개 소스와 제작자 설명/영상을 분석했다. 해당 커밋에는 라이선스 파일이 없어 소스·주석·자산을 복사하지 않았다. 피크 검출 원리는 기존 Digital Tennis/Zephyr 구조 위에 독립 구현했으며, 추가한 자체 코드에는 Apache-2.0을 적용한다. [분석·차이·적용 범위](docs/air-band-imu-adaptation.md)
