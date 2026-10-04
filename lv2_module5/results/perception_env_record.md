# 인지 PC 환경·카메라 기록

> 2026-10-01~02 측정의 대부분은 파이썬 프로토타입으로 수행했다. 이후 같은 알고리즘을 C++ 패키지 `ros2_ws/src/realsense`(`realsense::PerceptionNode`)로 옮겼고, 같은 실제 장면 3개에서 검출 여부·bbox·ex·면적비·탈락 사유가 파이썬과 동일함을 확인했다. 기본 설정은 `ros2_ws/src/realsense/config/realsense.yaml`이다. 파이썬 시절의 `min_area_px: 200`은 C++에서 `min_area_ratio: 0.0006`(640x480에서 약 184px)으로 바뀌었다.

기록일: 2026-10-01 · 기록자: 인지 담당

## 운용 목표 환경

| 장비 | OS | 역할 |
|---|---|---|
| 개발 PC | Ubuntu 24.04 + ROS 2 Lyrical | 개발·디버깅, bag 분석, 모니터링 |
| Raspberry Pi | **Ubuntu 26.04** | **운용: 카메라·인지·제어 노드 전부 + OpenCR 통신** |

발제의 Pi Ubuntu 버전(24.04)은 26.04로 수정될 예정이다(운영 측 안내). 인지 노드도 PC가 아닌 Pi에서 운용하는 것은 발제와 다르므로 report.md에 이유를 기록한다.

Pi에서 다시 확인할 항목: ROS 2 배포판, Python·numpy·OpenCV 버전, realsense2_camera arm64 설치, D435 USB 3 연결, 처리 FPS, CPU 사용률.

## 개발 PC·소프트웨어

| 항목 | 값 | 확인 방법 |
|---|---|---|
| OS | Ubuntu 24.04.5 LTS | `lsb_release -ds` |
| ROS 2 | Lyrical (`/opt/ros/lyrical`) | |
| Python | 3.12.3 (`.venv`, `--system-site-packages`) | `python --version` |
| numpy / OpenCV | 1.26.4 / 4.6.0 (apt 시스템 패키지) | `python -c "import numpy, cv2; ..."` |
| librealsense (pyrealsense2) | 2.58.4 (pip) | `rs.__version__` |
| realsense2_camera (wrapper) | 4.57.7 (apt `ros-lyrical-realsense2-camera`) | `ros2 pkg xml realsense2_camera` |

## 카메라

| 항목 | 값 | 확인 방법 |
|---|---|---|
| 모델 | **RealSense D435** (product id 0B07, IMU 없음) | `01_d435i_connection.py`, `lsusb` |
| 시리얼 | 261322072019 | `01_d435i_connection.py` |
| 펌웨어 | 5.15.1.55 | `camera_info.firmware_version` |
| USB | 3.2 | `01`, `02` |

발제 기준 장비는 D435i이지만 실제 연결된 장비는 D435(IMU 없음)다. 기본 과제는 Color·Depth만 사용하므로 영향이 없고, D435i IMU 확장은 이 장비로 수행할 수 없다.

## SDK 경로 확인 결과 (step_tests 01~03)

| 시험 | 결과 |
|---|---|
| 01 연결·컬러 수집 | 성공, shape (480, 640, 3), uint8, bgr8 요청 |
| 02 컬러 프로파일 | 320x180 ~ 1920x1080, 6/15/30/60 fps, bgr8·rgb8·yuyv 등 노출. 640x480 30fps 사용 가능 |
| 03 encoding (`--camera`) | bgr8, uint8, (480, 640, 3). 결과 이미지 `step_tests/results/03_*` |

## ROS wrapper 경로 확인 (개발 PC)

실행 명령:

```bash
ros2 launch realsense2_camera rs_launch.py \
    rgb_camera.color_profile:=640x480x30 \
    depth_module.depth_profile:=640x480x30 \
    align_depth.enable:=true
```

| 항목 | 값 |
|---|---|
| wrapper 실행 | 성공 (`RealSense Node Is Up!`, USB 3.2 인식) |
| 컬러 토픽 | `/camera/camera/color/image_raw`, 640x480 |
| **컬러 encoding** | **rgb8** (SDK 경로의 bgr8과 다름 → HSV 변환 시 `COLOR_RGB2HSV` 사용) |
| 컬러 frame_id | `camera_color_optical_frame` |
| CameraInfo K | fx=607.32, fy=607.19, cx=328.45, cy=256.01 (plumb_bob, 왜곡 계수 0) |
| 정렬 Depth 토픽 | `/camera/camera/aligned_depth_to_color/image_raw`, 16UC1, 640x480, frame_id `camera_color_optical_frame` |
| 실측 FPS (`ros2 topic hz`) | color 29.95 / depth 29.90 / aligned depth 29.93 |

참고:
- 시작 직후 `Depth stream start failure, Hardware Error` 경고가 1회 출력됐지만 이후 Depth·정렬 Depth 모두 약 30fps로 정상 발행됐다. 반복되는지 계속 관찰한다.
- `~/.realsense-config.json`이 없다는 ERROR 로그는 기본 설정을 쓴다는 의미이며 동작에 영향이 없었다.
- Depth 토픽을 `ros2 topic echo`로 볼 때는 `--qos-reliability best_effort --no-arr`가 필요했다.

### 영상 구독 QoS 비교 — 파이썬 rclpy 구독 (2026-10-01, 같은 PC, 640x480, 5초 평균)

| 구독 QoS | 컬러 수신 | 정렬 Depth 수신 |
|---|---|---|
| best-effort (`qos_profile_sensor_data`) | 7.0 fps | 0.0 fps |
| RELIABLE, depth 2 | 29.8 fps | 30.0 fps |

카메라 노드의 영상 발행 QoS는 RELIABLE이다. 큰 영상 메시지를 best-effort로 받으면 조각 손실로 대부분 버려졌다. 인지 노드의 **영상 구독은 RELIABLE**로 한다. `/target`처럼 작은 메시지의 best-effort 규약과는 별개다. Pi에서도 같은 비교를 다시 측정한다.

### C++ PerceptionNode 영상 구독 QoS 비교 (2026-10-02, 개발 PC)

`ros2 launch realsense realsense.launch.py` (realsense2_camera 640x480x30 + perception_node), 각 12초, 인지 노드 로그의 1초 처리 fps.

| `image_reliable` | 구독 QoS | 인지 노드 처리 fps |
|---|---|---|
| false | best-effort, depth 1 (report.md 인터페이스 표) | **1.0 / 1.0 / 2.0** |
| true | reliable, depth 1 | **30.0 × 5회** |

**결정 (2026-10-02, 인지 담당): 영상 구독은 reliable을 유지한다.** report.md 인터페이스 표("구독: best-effort · volatile · depth 1")는 수정하지 않고, 차이와 근거를 이 기록과 `ros2_ws/src/realsense/README.md`(설계 결정)에 남긴다. `/target` 발행은 report.md대로 best-effort다. `config/realsense.yaml`의 `image_reliable: false`로 바꾸면 best-effort로 비교 시험할 수 있다.

| 장비 | `image_reliable` | 인지 노드 처리 fps | 측정일 |
|---|---|---|---|
| 개발 PC | false (best-effort) | 1.0 / 1.0 / 2.0 | 2026-10-02 |
| 개발 PC | true (reliable) | 30.0 × 5회 | 2026-10-02 |
| Raspberry Pi 4 | false (best-effort) | | |
| Raspberry Pi 4 | true (reliable) | | |

## 조명 조건별 검출 확인

### 2026-10-02 · 낮, 블라인드 2/3 내림, 실내 소등 (창문 역광)

카메라 자동 노출 상태: exposure 166, gain 64, auto white balance 4600K, 30fps 유지.

| 영역 | H (5/50/95%) | S (5/50/95%) | V (5/50/95%) |
|---|---|---|---|
| 기둥 정면 | 105 / 105 / 106 | 236 / 249 / 255 | **37 / 39 / 42** |
| 기둥 윗면 테두리 | 101 / 106 / 108 | 68 / 182 / 223 | 48 / 62 / 111 |
| 흰 책상 | 13 / 30 / 38 | 9 / 14 / 23 | 106 / 112 / 118 |
| 회색 벽·블라인드 | 35 / 79 / 80 | 8 / 13 / 19 | 144 / 150 / 154 |
| 모니터 받침(어두운 물체) | 47 / 80 / 98 | 15 / 81 / 191 | 8 / 16 / 111 |

- 원인: 기둥 정면 V ≈ 39 < 기존 V 하한 50 → 미검출 (마스크에 윗면 테두리만 남아 `small`로 탈락).
- V 하한별 비교 (같은 장면): 50 → 기둥 3% 채움·미검출 / 35~10 → 기둥 83% 채움·검출, 기둥 밖 마스크 픽셀 46개로 변화 없음.
- 조치: V 하한 **50 → 20**. 측정값 39 대비 여유를 두되, 매우 어두운 픽셀(V<20)의 불안정한 H는 제외한다.
- 역광 보정(`backlight_compensation`) 비교: 끔 → 기둥 V 40 / 켬 → 기둥 V 55~60, S 249 → 234, fps 30 유지.
- 파이프라인 결과 (rs_read + perception, 9초):

| 역광 보정 | 검출 | bbox | 중심 HSV |
|---|---|---|---|
| 끔 | 30/30 | (294,337) 50x100 | [105, 249, 40] |
| 켬 | 30/30 | (294,337) 50x100 | [106, 234, 60] |

- 근거 장면: `ros2_ws/src/realsense/test/data/dim_backlit_20261002.png` (gtest `Detector.RealDimBacklitScene`)
- 밝은 조건은 V 하한 변경 후 다시 확인해야 한다 (V 하한을 낮추면 마스크가 넓어지므로 배경 오검출 여부 확인).

## Raspberry Pi 4 환경 (운용 장비)

기록일: 2026-10-02

| 항목 | 값 | 개발 PC와 비교 |
|---|---|---|
| OS | Ubuntu 26.04.1 LTS | PC 24.04.5 |
| CPU 아키텍처 | aarch64 (arm64) | PC x86_64 |
| ROS 2 | Lyrical (`/opt/ros/lyrical`) | 같음 |
| Python | 3.14.4 | PC 3.12.3 |
| numpy | 2.3.5 | PC 1.26.4 |
| OpenCV | 4.10.0 | PC 4.6.0 |
| rclpy·yaml import | 성공 | |
| realsense2_camera (wrapper) | 4.58.4 (apt) | PC 4.57.7 |
| wrapper 실행 (컬러만, 640x480x30) | 성공, `ros2 topic hz` 평균 30.0~30.5 fps, 프레임 간격 0.017~0.046 s | PC 29.95 fps |
| CPU (`perception.launch.py`, 컬러만, `top -b -n 1` 1회) | 인지 노드 python3 72.7%, realsense2_camera_node 27.3% (코어 1개 = 100%), 전체 idle 72.7%, 메모리 656 MiB 사용 / 3779 MiB | 1회 측정값, 재측정 필요 |
| 인지 노드 (`perception.launch.py`, 기둥 검출 중) | fps 30.0, proc 5.9~6.0 ms, age 47~48 ms, detected 30/30, bbox (325,295) 40x74, HSV [107~108, 238~239, 77~79] | PC proc 1.2 ms, age 45 ms |

| D435 연결 | Bus 002, `8086:0b07`, uvcvideo, **5000M (USB 3.0)** | PC USB 3.2 |
| 같은 Pi의 다른 USB 장치 | Bus 001: `cdc_acm` 12M, `cp210x` 12M (OpenCR·USB 시리얼 변환기로 추정, 제어 담당 확인) | |

numpy 1.x → 2.x, OpenCV 4.6 → 4.10 차이가 있으므로 Pi에서 `tests/test_detector.py`를 다시 실행해 확인했다: **15/15 passed** (실제 어두운 장면 회귀 테스트 포함). 코드는 PC에서 rsync로 복사(`.venv`·`captures` 제외).

### 2026-10-02 · 하늘색 반투명 판 옆의 파란 기둥 (밝은 실내)

증상: 디버그 화면에서 하늘색 판 아래쪽이 기둥 대신 선택됨 (중심 HSV [100, 199, 113], 면적 7646px > 기둥 약 6100px).
같은 장면을 다시 찍어 영역별 HSV를 측정했다 (`ros2_ws/src/realsense/test/data/panel_and_pillar_20261002.png`, 1/5/50/95/99 백분위).

| 영역 | H | S | V |
|---|---|---|---|
| 파란 기둥 | 102 / 107 / 109 / 109 / 109 | 63 / 255 / 255 / 255 / 255 | 19 / 99 / 106 / 113 / 123 |
| 하늘색 판 전체 | 97 / 97 / 99 / 100 / 100 | 109 / 117 / 178 / 199 / 203 | 110 / 111 / 117 / 136 / 146 |
| 하늘색 판 아래쪽 1/3 | 98 / 99 / 100 / 100 / 101 | 150 / 176 / 191 / 201 / 209 | 110 / 111 / 120 / 143 / 157 |
| (참고) 기둥, 어두운 역광 | 104 / 105 / 105 / 106 / 107 | 233 / 236 / 249 / 255 / 255 | 37 / 37 / 39 / 42 / 46 |

- 원인: 어두움이 아니라 **비슷한 색 물체 선택**. 판 아래쪽(H 100, S ≤ 209)이 기존 하한(H 100, S 150)을 통과했고 기둥보다 면적이 커서 선택됐다.
- 조치: `hsv_lower` [100, 150, 20] → **[103, 220, 20]**. 판은 H ≤ 101·S ≤ 209, 기둥은 H ≥ 104·S ≥ 233으로 H·S 모두 분리된다.

| 설정 | 판 영역 마스크 | 기둥 영역 마스크 | 선택된 대상 | 어두운 역광 장면 |
|---|---|---|---|---|
| [100, 150, 20] | 18.9~20.2% | 97.0% | 기둥 (이 프레임) | 검출 |
| [103, 220, 20] | **0.0%** | 96.2~96.3% | 기둥 | 검출 |

- 남은 한계: 어두울 때의 남색 물체(H 106, S 240, V 36)는 기둥과 색으로 구분되지 않는다 (모양·크기 조건이 필요).

### 2026-10-02 · 조명을 비춘 파란 기둥 (HSV 범위 2 추가)

증상: H 103·S 220으로 바꾼 뒤, 어두울 때는 검출되지만 조명을 비추면 기둥을 놓쳤다.
실행 중인 카메라 토픽에서 8프레임을 받아 측정했다 (`ros2_ws/src/realsense/test/data/lit_pillar_20261002.png`, 5/50/95 백분위).

| 대상 | H | S | V | 범위 1([103,220,20])만 쓸 때 기둥 마스크 |
|---|---|---|---|---|
| 기둥, 조명 없음 (2프레임) | 105 / 106 / 107 | 248 / 255 / 255 | 114 / 125 / 134 | 99.5~99.7% |
| 기둥, 조명 비춤 (6프레임) | 104 / 105 / 107 | 170~183 / 191~202 / 249~253 | 141~151 / 171~176 / 175~181 | **7.4~12.3%** |
| 하늘색 판, 조명 비춤 | 89~95 / 98~99 / 103~104 (1/50/99) | 72~84 / 127~157 / 164~200 | 18~34 / 134~168 / 174~200 | — |

- 원인: 조명 아래에서 기둥의 S가 170~202로 내려가고 V가 오른다. 같은 조건의 판도 S 127~200이라 S만으로는 둘을 나눌 수 없다. H는 여전히 분리된다 (판 중앙값 98, 기둥 105).
- 조치: HSV 범위 2 추가 `hsv_bright_lower: [103, 150, 130]` (마스크 = 범위 1 OR 범위 2). 어두운 픽셀은 범위 1의 높은 S 조건을 그대로 쓰고, 밝은 픽셀(V ≥ 130)만 S 150까지 받는다.

| 설정 | 조명 비춤 6프레임 | 조명 없음 2프레임 | 판 옆 기둥 장면 | 어두운 역광 장면 | 기둥 밖 마스크 픽셀 (조명 비춤) |
|---|---|---|---|---|---|
| 범위 1개 [103,220,20] | 미검출 5 / 오검출 1 (작은 조각) | 검출 | 검출 | 검출 | — |
| 범위 1개 [103,160,20] | 검출 6 | 검출 | 검출 | 검출 | 630~655 |
| **범위 2개 [103,220,20] + [103,150,130]** | **검출 6** | 검출 | 검출 | 검출 | **429~430** |

범위 2개 방식이 범위 1개(S 160)보다 기둥 밖 마스크 픽셀이 약 30~80% 적어(판 옆 기둥 장면 48 → 9) 이 방식을 택했다.
