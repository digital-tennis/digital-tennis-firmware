# 가상 IMU 테니스 실험실

**실제 센서 없이 라켓 궤적에서 6축 값을 만들고, 펌웨어와 동일한 C 코드에 재생합니다.** 생성할 동작과 판정 결과를 나란히 볼 수 있습니다. 사람이 휘두른 데이터의 정확도를 검증하는 도구는 아닙니다.

```sh
# firmware 저장소 루트에서 실행
cmake -S . -B .work/motion-simulator
cmake --build .work/motion-simulator
ctest --test-dir .work/motion-simulator --output-on-failure
python3 simulator/server.py --port 8766
```

브라우저에서 <http://127.0.0.1:8766/>을 엽니다.

![가상 라켓과 센서값을 C 판정기로 재생한 실험실](../docs/tennis-motion-simulator.jpg)
 Python 표준 라이브러리만 사용하며 서버는 로컬 인터페이스에만 바인딩합니다. 중지는 터미널에서 Ctrl+C입니다. BLE·실제 장치·Unity를 조작하지 않습니다. protocol 소스가 로컬에 있으면 CMake 첫 설정에 `-DDT_PROTOCOL_ROOT=/absolute/path/digital-tennis-protocol`을 추가할 수 있습니다.

1. 가상 동작과 경기 상황을 고릅니다. 서브·스매시는 상황을 모르면 모호함으로 거부합니다.
2. 사용 손·장착 회전·속도·범위·잡음을 바꿔 실행합니다. 같은 재현 번호는 같은 잡음을 만듭니다.
3. 재생/일시정지와 타임라인으로 라켓·부착 센서를 관찰합니다. 이 위치는 센서로 추정한 위치가 아니라 생성기의 참값입니다.
4. 가속도·자이로 그래프, C 판정, 후보, 세기, 피크 이후 판정 지연과 20바이트 패킷을 확인합니다.
5. **센서 CSV 저장**은 시각과 6축 값만 내보냅니다. 기대 동작 이름은 C 분류기로 전달되지 않습니다.

단일 재생과 고정 평가도 가능합니다.

```sh
python3 simulator/run.py --motion slice --context rally --left --mount 90 --out .work/slice.json
python3 simulator/evaluate.py --repetitions 100 --validation-seed 731091 --out .work/motion-validation.json
```

평가에는 약간의 시간이 걸립니다. 준비 표본과 별도 시드 표본, 혼동 행렬, 누락·중복·오검출·판정 지연을 기록합니다. 현재 CSV를 자동 학습하거나 평가 결과에 맞춰 조정하지 않습니다.

궤적은 [motions.csv](motions.csv), 판정 범위는 [gesture-rules.csv](../config/gesture-rules.csv)입니다. 범위를 수정하면 **CMake 재빌드**가 필요합니다. CSV가 펌웨어에 런타임 로드되거나 BLE로 전송되는 기능은 없습니다.

기본 보드 앱은 기존 빠른 가속도 피크 프로필을 유지합니다. 실험용 7종 프로필은 스윙 종료를 기다려 추가 지연이 있고, 경기 상황을 보드에 보내는 BLE 명령도 아직 없습니다. [설계·수정 위치·실물 인수 가이드](../docs/tennis-gesture-simulator.md)와 [검증 기록](../docs/gesture-simulator-validation.json)을 함께 읽어 주세요.

## 실험으로 임계값 찾기

[선택 기준·고정 시드·최종 전후 비교](../docs/threshold-experiment.md)에 따라 `threshold_study.py tune`으로60개 후보를 비교하고, 선택한 CSV를 적용·재빌드한 다음 `evaluate`로 분리한 최종 표본을 엽니다. 실험을 반복할 때는 새로운 출력 폴더를 사용합니다. `config/gesture-detector.csv`와 `config/gesture-rules.csv`를 CMake가 읽으며 기존 빠른 보드 프로필은 유지합니다.
