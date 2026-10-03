# CONTROL — 인지 → 제어 실행 명령 (가상 모터)

실제 RealSense D435 영상으로 인지하고, 모터만 가상 OpenCR로 대신하여 제어까지 확인하는 절차입니다.

```
D435(실제) → realsense2_camera → perception_node ─/target→ dynamixel_move_node ─/motor_cmd→ dynamixel_controller ─가상 시리얼→ opencr_sim (가상 OpenCR·pan·tilt)
```

| 구분 | 위치 |
|---|---|
| 시험 작업공간 | `~/test_ws` (저장소 밖) |
| `realsense` (인지) | `~/test_ws/src/realsense` — `origin/feature/perception` 사본 |
| `dynamixel` (제어) | `~/test_ws/src/dynamixel` → 저장소 `lv2_module5/ros2_ws/src/dynamixel` 링크 (`feature/control`) |
| `bringup` (launch·Python 도구) | `~/test_ws/src/bringup` — `origin/docs/documents` 사본 + `record_bag`·`analyze_bag`·`opencr_sim` |
| 가상 모터용 제어 설정 | `~/test_ws/dynamixel_sim.yaml` (`serial_port: /tmp/opencr_sim`) |
| 실행 로그 | `~/test_ws/logs/` |
| bag | `~/test_ws/rec/` |

모든 터미널에서 먼저 실행합니다.

```bash
cd ~/test_ws && source install/setup.bash
```

## 1. 빌드 (코드를 바꿨을 때)

```bash
cd ~/test_ws
source /opt/ros/lyrical/setup.bash
colcon build --symlink-install --parallel-workers 1 --cmake-args -DBUILD_TESTING=OFF
source install/setup.bash
```

| 상황 | 명령 |
|---|---|
| 제어(C++)만 다시 빌드 | `colcon build --symlink-install --packages-select dynamixel` |
| Python 도구만 다시 빌드 | `colcon build --symlink-install --packages-select bringup` |

## 2. 실행

### 2.1 터미널 1 — 가상 OpenCR

```bash
ros2 run bringup opencr_sim
```

- `/tmp/opencr_sim` 가상 시리얼 포트를 만들고 `READY`를 보냅니다.
- 펌웨어와 같은 규칙: `V <pan> <tilt>` / `S`, 속도 상한 0.5 rad/s, 500 ms 명령 없으면 `TIMEOUT` 정지, pan 135~225° · tilt 150~210° 끝에서 `LIMIT`.
- 초기 각도 변경: `ros2 run bringup opencr_sim --pan 200`

### 2.2 터미널 2 — 카메라 + 인지 + 제어

```bash
ros2 launch bringup bringup.launch.py use_motor:=true \
  dynamixel_params:=$HOME/test_ws/dynamixel_sim.yaml
```

- `dynamixel_controller`가 `/tmp/opencr_sim`에 연결됩니다. 가상 OpenCR을 나중에 켜도 1초마다 다시 연결합니다.
- 실행 시 `Parameter 'use_motor' is not supported` 같은 노란 경고는 realsense2_camera launch가 출력하는 것으로 동작에는 영향이 없습니다.
- 모터 출력 없이(컨트롤러 없이) 실행: `use_motor:=false`

## 3. 확인 (터미널 3)

```bash
ros2 node list                                        # camera, perception_node, dynamixel_move_node, dynamixel_controller, opencr_sim
ros2 topic hz /camera/camera/color/image_raw          # 약 30 Hz
ros2 topic echo /target --qos-reliability best_effort --field point   # x = 좌우 오차, z = 0 이면 미검출
ros2 topic echo /tracking_status                      # IDLE · TRACKING · LOST
ros2 topic echo /motor_cmd --field velocity           # [pan, tilt] rad/s
ros2 topic echo /opencr_sim/joint_states --field position             # 가상 모터 각도 [rad] (π = 180°)
```

시험 순서 (파란 사각 기둥, 20cm~1m):

| 동작 | 기대 결과 |
|---|---|
| 기둥을 화면 중앙에 | `TRACKING`, pan 속도 0 (데드밴드 0.05) |
| 기둥을 오른쪽으로 | pan 속도 +, 가상 pan 각도 증가 |
| 기둥을 왼쪽으로 | pan 속도 −, 가상 pan 각도 감소 |
| 기둥을 가림 | 첫 미검출 프레임에 `LOST`, 속도 0 |
| 다시 보이게 | 연속 3프레임 검출 후 `TRACKING` 복귀 |

- 실제 카메라는 돌지 않으므로 열린 루프입니다. 기둥이 한쪽에 계속 있으면 가상 pan은 범위 끝까지 돌고 `LIMIT pan`으로 멈춥니다.
- `direction`·`kp`는 실행 중에 바꿀 수 있습니다: `ros2 param set /dynamixel_move_node kp 0.3`

인지 디버그 영상(박스·중심 표시)은 `/perception_node/debug_image/compressed`로 발행됩니다.

## 4. 기록 (rosbag2, 터미널 3)

```bash
mkdir -p ~/test_ws/rec
ros2 run bringup record_bag success --out-dir ~/test_ws/rec --duration 20 \
  --topics /camera/camera/color/image_raw /target /motor_cmd /tracking_status /opencr_sim/joint_states
```

- 출력: `rec/<YYYYMMDD_HHMMSS>_<장면>/` (`*.db3` + `metadata.yaml`), `rec/<RUN_ID>.sha256`
- 장면 이름: `success` (정상 추적) · `lost` (가림 후 재등장)
- `--duration` 생략 시 `Ctrl+C`까지 기록. 토픽 탐색에 2~4초가 걸려 bag 기간은 지정 시간보다 짧습니다.
- 영상 포함 시 12초에 약 240MB입니다. 남은 공간 확인: `df -h ~`

## 5. 분석

```bash
ros2 run bringup analyze_bag ~/test_ws/rec/<RUN_ID> --csv ~/test_ws/rec/<RUN_ID>.csv
```

- 출력: 처리 FPS, 검출 비율(z > 0), 수평 RMSE, 추적 구간 비율, 상태 전이, 소실·복구 시간, `/target` 공백(입력 중단)
- CSV 컬럼: `run_id, time_s, frame_id, detected, ex, ey, area_ratio, state, command, command_unit`

## 6. 안전 동작 시험

| 시험 | 명령 | 기대 결과 |
|---|---|---|
| 인지 입력 중단 (TRACKING 중) | `pkill -f perception_node` | 0.5초 후 `LOST`, 로그에 `/target 입력 중단 (타임아웃)` |
| 제어 명령 중단 | `pkill -f dynamixel_move_node` | 컨트롤러 로그 `/motor_cmd 0.50s 이상 미수신`, 가상 모터 속도 0 |
| 시리얼 명령 중단 | `pkill -f dynamixel_controller` | 가상 OpenCR 로그 `→ TIMEOUT`, 속도 0 |

## 7. 종료

```bash
# 각 터미널에서 Ctrl+C. 남은 프로세스 정리:
pkill -f "ros2 launch bringup"; pkill -f opencr_sim
pkill -f realsense2_camera_node; pkill -f perception_node; pkill -f dynamixel_
pgrep -af "realsense2|perception|dynamixel|opencr_sim"   # 아무것도 나오지 않으면 종료 완료
```
