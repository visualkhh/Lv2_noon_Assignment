# realsense — 인지 패키지 (C++)

파란 사각 기둥(30×30×60mm, 20cm~1m)을 RealSense D435 컬러 영상에서 HSV·Contour로 검출해 `/target`을 발행한다.
카메라 영상은 Intel 공식 래퍼 `realsense2_camera`가 발행하고 `realsense.launch.py`가 include한다.

## 노드

| 항목 | 값 |
|---|---|
| 실행 파일 / 노드 이름 | `perception_node` |
| 컴포넌트 | `realsense::PerceptionNode` |
| 구독 | `image_topic` 파라미터, 기본 `/camera/camera/color/image_raw` (`sensor_msgs/Image`) |
| 발행 | `target_topic` 파라미터, 기본 `/target` (`geometry_msgs/PointStamped`, best-effort · volatile · depth 1) |
| 디버그 발행 | `debug_image_topic`·`mask_topic` 파라미터, 기본 `/perception_node/debug_image/compressed`, `/perception_node/mask/compressed` (JPEG, `debug_rate_hz`) |
| 로그 | `log_period_s`마다 처리 fps · proc · age · 검출 수 · bbox · 중심 · 중심 픽셀 BGR/HSV |

### 검출 처리 (문제 1)

| 단계 | 내용 | 코드 |
|---|---|---|
| 1. 영상 변환 | `rgb8` → BGR (`cv_bridge`) | `perception_node.cpp` |
| 2. HSV 변환·마스크 | 범위 1 OR 범위 2 (`hsv_lower/upper`, `hsv_bright_*`) | `detector.cpp` `make_mask` |
| 3. 잡음 제거 | open(`open_kernel`) → close(`close_kernel`) | `make_mask` |
| 4. 컨투어 | `findContours(RETR_EXTERNAL)`, 20px 미만 점은 후보에서 제외 | `detect` |
| 5. 후보 필터 | 면적비 `min_area_ratio`~`max_area_ratio`, `aspect_max`, `fill_min`, `shape_score_min`. 탈락 사유 `small`·`large`·`aspect`·`fill`·`shape` | `detect` |
| 6. 대상 선택 | `select_rule: shape`(기본): 조건을 모두 통과한 후보 중 **모양 점수가 가장 높은 것** (점수 차 0.02 이내면 면적이 큰 쪽). 직전에 고른 목표에 가장 가까운 후보 하나는 점수에 `track_bonus`를 더해 비교하고 동률이면 먼저 고른다 (추적 유지, 인지 노드만). `select_rule: area`: **면적이 가장 큰 것** (예전 방식) | `detect` |
| 7. 중심 계산 | 컨투어 모멘트 무게중심 (cx, cy). `m00 = 0`이면 미검출 | `detect` |
| 8. 표시 | 원본 위에 영상 중심 십자(회색), 선택 대상 박스(초록)·윤곽(하늘색)·중심(초록 점), 탈락 후보(빨강 + 사유) | `draw` → `~/debug_image` |

이전 프레임 정보는 쓰지 않는다. 미검출이면 이전 좌표를 재사용하지 않고 z = 0을 발행한다.

### /target

| 필드 | 의미 |
|---|---|
| `point.x` | ex = (cx − W/2) / (W/2), 오른쪽 + (−1~+1). cx는 컨투어 무게중심 |
| `point.y` | ey = (cy − H/2) / (H/2), 아래쪽 + (−1~+1) |
| `point.z` | 면적비 = contour_area / (W×H). **0이면 미검출** (x·y를 쓰지 않는다) |
| `header` | 원본 영상의 stamp·frame_id 그대로 |

영상 1장마다 발행하고, 미검출이어도 z=0으로 발행한다. 카메라가 멈추면 발행하지 않는다 (이전 좌표 재사용 금지).

### 설계 결정: 영상 구독 QoS는 reliable

| 항목 | 내용 |
|---|---|
| 결정 | 카메라 영상(`image_topic`) 구독은 **reliable · volatile · depth 1** (`image_reliable: true`) |
| report.md와의 차이 | report.md 구조도·토픽 정의·QoS 범례는 영상 구독을 best-effort로 적고 있다. report.md는 수정하지 않고, 차이와 근거를 이 문서와 측정 기록에 남긴다 |
| 적용 범위 | 카메라 영상 구독만. `/target` 발행은 report.md대로 **best-effort** · volatile · depth 1 |
| 근거 | 같은 카메라·해상도(640x480x30)에서 인지 노드 처리 fps를 비교 (아래 표) |
| 추정 원인 | 640x480 컬러 1장 ≈ 900KB라 여러 조각으로 나뉘어 전송된다. best-effort는 조각 하나만 잃어도 프레임 전체가 버려지는 것으로 보인다. 카메라 노드는 reliable로 발행하므로 reliable 구독이면 재전송으로 모두 받는다 |
| 되돌리기 | 파라미터 파일에서 `image_reliable: false` → report.md 표대로 best-effort 구독 (비교 시험용) |
| 결정일·담당 | 2026-10-02 · 인지 담당 |

| 장비 | 구독 QoS | 인지 노드 처리 fps | 기록 |
|---|---|---|---|
| 개발 PC (Ubuntu 24.04) | best-effort | 1.0 / 1.0 / 2.0 | `results/perception_env_record.md` |
| 개발 PC (Ubuntu 24.04) | reliable | 30.0 × 5회 | `results/perception_env_record.md` |
| Raspberry Pi 4 (Ubuntu 26.04) | best-effort | (측정 예정) | |
| Raspberry Pi 4 (Ubuntu 26.04) | reliable | (측정 예정) | |

실행 중인 노드의 실제 QoS는 시작 로그(`PerceptionNode started | 구독 ... (reliable)`)와 `ros2 topic info -v /camera/camera/color/image_raw`로 확인한다.

## 파라미터 (`config/realsense.yaml`)

| 파라미터 | 기본 | 의미 |
|---|---|---|
| `image_topic` | `/camera/camera/color/image_raw` | 구독할 카메라 컬러 토픽 (report.md 구조도) |
| `target_topic` | `/target` | 발행할 목표 토픽 (report.md 구조도) |
| `debug_image_topic` | `/perception_node/debug_image/compressed` | 디버그 영상 토픽 |
| `mask_topic` | `/perception_node/mask/compressed` | 마스크 영상 토픽 |
| `hsv_lower`, `hsv_upper` | `[103,220,20]`, `[130,255,255]` | HSV 범위 1 (어두움·보통): S가 높은 픽셀만. V 하한 20으로 어두운 역광의 기둥까지 포함, H·S 하한으로 하늘색 판·셔츠 제외 |
| `hsv_bright_enabled` | true | HSV 범위 2 사용 여부. 마스크 = 범위 1 OR 범위 2 |
| `hsv_bright_lower`, `hsv_bright_upper` | `[103,150,130]`, `[130,255,255]` | HSV 범위 2 (조명 받은 밝은 면): 빛이 섞여 S가 내려간 기둥을 받되 밝은 픽셀(V ≥ 130)만. 측정값은 `config/realsense.yaml` 주석 |
| `min_area_ratio` | 0.0006 | 최소 면적비 (1m 기둥 ≈ 0.0021, 일부 가림 고려) |
| `max_area_ratio` | 0.15 | 최대 면적비 (20cm 기둥 ≈ 0.054~0.075) |
| `open_kernel`, `close_kernel` | 3, 5 | 잡음 제거 커널(px), 0이면 생략 |
| `aspect_max` | 4.0 | 회전 사각형 긴 변/짧은 변 상한 |
| `fill_min` | 0.6 | 컨투어 면적 / 회전 사각형 면적 하한 |
| `select_rule` | `shape` | 후보 선택 규칙. `shape` = 모양 점수 최고, `area` = 면적 최대 (예전 방식) |
| `shape_score_min` | 0.0 | 모양 점수 하한 (0이면 끔). 모양 점수 = fill × (회전 사각형 둘레 / 컨투어 둘레)², 매끈한 사각형 = 1 |
| `track_bonus` | 0.05 | 추적 유지 가산점 (0이면 끔). 직전 목표 중심에 가장 가까운 후보 하나의 모양 점수에 더한다. 선택에만 쓰고 미검출 프레임에 이전 좌표를 발행하지 않는다 |
| `track_radius` | 0.15 | 가산점 후보의 최대 거리 (직전 중심과의 거리 / 영상 폭) |
| `track_hold_frames` | 5 | 이 프레임 수보다 오래 놓치면 직전 목표를 잊는다 |
| `image_reliable` | true | 영상 구독 QoS (true = reliable, false = best-effort). 위 설계 결정 참고 |
| `debug_rate_hz`, `jpeg_quality` | 5.0, 70 | 디버그 영상 발행 주기·품질 (0이면 끔) |
| `probe_x`, `probe_y` | −1 | 0 이상이면 그 픽셀의 BGR·HSV를 로그에 남김 |
| `log_period_s` | 1.0 | 상태 로그 주기 |

검출·디버그 파라미터는 실행 중에 바꿀 수 있다 (잘못된 값은 거부된다).

```bash
ros2 param set /perception_node hsv_lower "[103, 210, 20]"
ros2 param set /perception_node hsv_bright_lower "[103, 140, 130]"
ros2 param set /perception_node hsv_bright_enabled false
ros2 param set /perception_node probe_x 320
ros2 param set /perception_node probe_y 240
```

시험에 쓴 설정은 `lv2_module5/config/realsense_<설명>.yaml`로 복사해 `params_file:=`로 지정한다 (config/README.md).

## 빌드·테스트

```bash
cd lv2_module5/ros2_ws
source /opt/ros/lyrical/setup.bash
colcon build --symlink-install --packages-select realsense
source install/setup.bash
colcon test --packages-select realsense && colcon test-result --verbose
```

테스트: gtest 18개 (합성 영상, 실제 장면 3개 — 어두운 역광 `dim_backlit_20261002.png`, 하늘색 판 옆 기둥 `panel_and_pillar_20261002.png`, 조명 받은 기둥 `lit_pillar_20261002.png` — 파라미터 파일, depth.npy) + ament lint.

C++ 빌드 의존성: `sudo apt install libopencv-dev libyaml-cpp-dev ros-lyrical-cv-bridge ros-lyrical-rclcpp-components ros-lyrical-ament-cmake-gtest` (또는 `rosdep install --from-paths src -y --ignore-src`).

## 실행

```bash
ros2 launch realsense realsense.launch.py                                   # 카메라(424x240x30) + perception_node
ros2 launch realsense realsense.launch.py color_profile:=640x480x30         # 해상도 바꿔 실행 (비교 시험용)
ros2 launch realsense realsense.launch.py params_file:=$PWD/../config/realsense_<설명>.yaml
ros2 launch realsense realsense.launch.py use_camera:=false                 # bag 재생으로 입력할 때
ros2 topic echo /target --qos-reliability best_effort --field point
```

토픽 이름은 용도에 따라 실행할 때 바꾼다 (테스트·bag 재처리·래퍼 버전 차이). 코드·빌드는 그대로다.

```bash
# launch 인자 (비우면 params_file의 값 사용)
ros2 launch realsense realsense.launch.py image_topic:=/camera/color/image_raw
ros2 launch realsense realsense.launch.py use_camera:=false target_topic:=/target_replay
# 노드 단독 실행
ros2 run realsense perception_node --ros-args --params-file src/realsense/config/realsense.yaml \
  -p target_topic:=/target_replay -p use_sim_time:=true
# 디버그 영상 토픽을 바꿨다면 뷰어도 같은 이름으로
ros2 run realsense view_debug --ros-args -p debug_image_topic:=<토픽> -p mask_topic:=<토픽>
```

토픽 이름은 시작 시 정해지므로 `ros2 param set`으로는 바뀌지 않는다 (재시작 필요). 기존 remap 방식(`-r /target:=/target_replay`)도 그대로 동작한다.

카메라 센서 설정은 실행 중 `realsense2_camera` 파라미터로 바꾼다. 예: 창문 역광에서 기둥이 어두우면

```bash
ros2 param set /camera/camera rgb_camera.backlight_compensation true   # 측정: 기둥 V 약 40 → 55~60
```

## 도구

| 실행 파일 | 용도 |
|---|---|
| `ros2 run realsense view_debug` | 박스 그린 디버그 영상·마스크 보기 (화면 없는 Pi 결과를 PC에서 확인). `s` 저장, `q` 종료 |
| `ros2 run realsense hsv_tuner --label 50cm` | 슬라이더 HSV 튜닝 (범위 1 + `B ...` 범위 2), 클릭 위치 HSV 표시, `s` 장면 저장, `w` 현재 값을 `realsense_tuned.yaml`(파라미터 파일)로 저장. 카메라 노드를 `align_depth.enable:=true`로 켜면 Depth도 저장. `--snapshot`은 화면 없이 1장 |
| `ros2 run realsense rerun_captures captures/* --config <파라미터 파일>` | 저장 장면을 다른 설정으로 재검출해 저장 당시와 비교 (트러블슈팅) |
| `ros2 run realsense ros_graph` | 노드·토픽·QoS 연결 그림 (rqt_graph 대용, `graphviz` 필요) |

`hsv_tuner`·`view_debug`·`ros_graph`는 현재 폴더의 `captures/`에 저장한다 (`.gitignore` 대상).

## 주의

- realsense2_camera 컬러 encoding은 **rgb8**이다. `cv_bridge`로 bgr8로 변환한 뒤 처리한다.
- 카메라 노드와 perception_node는 한 장비에서만 실행한다. PC와 Pi에서 동시에 켜면 `/target`이 섞인다.
- Lyrical의 `uint8[]` 메시지 필드는 `rosidl::Buffer`다 (`data()`·`size()`·`resize()` 사용).

## 변경 이력

| 날짜 | 변경 | 이유·요청 | 확인 |
|---|---|---|---|
| 2026-10-02 | `realsense` 패키지 구현: HSV·Contour 검출, `/target` 발행, 디버그 영상, 도구 4개, gtest | 문제 1·2 | 커밋 `8d2693a` |
| 2026-10-02 | HSV 범위 2개 (범위 1 어두움·보통 + 범위 2 조명 받은 면) | 조명을 비추면 기둥 S가 170~202로 내려가 미검출 | `results/perception_env_record.md`, gtest `RealLitPillarScene` |
| 2026-10-02 | 영상 구독 QoS reliable 유지 (`image_reliable: true`) | 실측 best-effort 1~2fps / reliable 30fps | 위 "설계 결정" |
| 2026-10-04 | 토픽 이름 파라미터화: `image_topic`, `target_topic`, `debug_image_topic`, `mask_topic` (기본값 = report.md 구조도). `realsense.launch.py`에 `image_topic`·`target_topic` 인자 (remap → 파라미터 전달), `view_debug`도 토픽 파라미터 | 팀장 요청: 테스트 상황에 따라 토픽명을 바꿀 수 있게 | 실카메라로 기본값·launch 인자·`-p`·기존 `-r` remap·없는 토픽 경고 확인, colcon test 55개 0 실패 |
| 2026-10-04 | 실행 중 `ros2 param set`으로 토픽 이름·`image_reliable`을 바꾸면 거부하고 이유를 알림 | 반영되지 않는데 성공으로 보이던 문제 | `Setting parameter failed: ... 실행 중에 바꿀 수 없다` 확인 |
| 2026-10-04 | `realsense.launch.py` docstring 형식 수정 | ament pep257(D213) 실패 해결 | pep257 통과 |
| 2026-10-06 | 후보 선택 규칙 `select_rule: shape` 추가 (기본), 모양 점수 `shape_score`·하한 `shape_score_min` 추가, 로그·디버그 화면에 `shape=` 표시 | 사각 기둥과 원기둥(같은 파란색)이 함께 보이면 면적이 큰 원기둥을 고름. 원기둥은 곡면 음영으로 윤곽이 울퉁불퉁해 모양 점수가 낮음 (사각 0.83~0.87, 원기둥 0.42~0.80) | 디버그 화면 6장: 예전 규칙 4장 원기둥 선택 → 새 규칙 6장 모두 사각 기둥. gtest `ShapeRulePrefersPillarOverLargerCylinder` (실제 실루엣 2장) |
| 2026-10-06 | 추적 유지 `track_bonus`·`track_radius`·`track_hold_frames` 추가 (기본 0.05·0.15·5). 인지 노드 디버그 영상은 선택한 목표만 표시 (탈락 후보 빨간 윤곽은 `hsv_tuner`·`rerun_captures`에서만) | 사각 기둥과 원기둥이 함께 보이면 번갈아 선택됨. 424x240에서는 원기둥 윤곽이 매끈해져 모양 점수가 비슷함 (사각 0.92~0.98, 원기둥 0.74~0.96) → 0.02 동률 규칙으로 면적이 큰 원기둥이 가끔 선택 | 424x240 실카메라 337프레임 재검출: 선택 변경 18회 → 0회. 원기둥에서 시작해도 1프레임 만에 사각 기둥으로 이동. gtest 3개 추가 (실제 프레임 `pillar_cylinder_424x240_20261006.png` 포함), colcon test 60개 0 실패 |
| 2026-10-06 | 카메라 컬러 기본 해상도 `640x480x30` → **`424x240x30`** (`realsense.launch.py` `color_profile` 기본값). 면적비 근거 주석·`hsv_tuner` 안내·gtest(424x240 1m·20cm) 갱신 | 원본 영상 크기 축소 (1장 0.92MB → 0.31MB, 27MB/s → 9.2MB/s). 팀원 혼동·통합 시 해상도 불일치 방지를 위해 실행 인자가 아닌 기본값으로 지정. 팀 `bringup.launch.py`는 `use_camera`만 넘기므로 이 기본값을 그대로 쓴다 | `ros2 topic bw` 0.31MB, 1m 정면 479/479 검출(면적비 0.0014, 하한의 2.3배), 인지 노드 CPU 21% → 9%, colcon test 0 실패 |
