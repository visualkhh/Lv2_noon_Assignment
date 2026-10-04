# CONTROL — 인지 → 제어 실행 명령 (가상 환경)

실제 RealSense·모터 없이, 팀 노드(`perception_node`, `dynamixel_move_node`, `dynamixel_controller`)를 그대로 실행하고 카메라·OpenCR만 가상으로 대신합니다.

```
camera_sim ─/camera/camera/color/image_raw→ perception_node ─/target→ dynamixel_move_node ─/motor_cmd→ dynamixel_controller ─/tmp/opencr_sim→ opencr_sim
    ↑                                                                                                                                   │
    └──────────────────────────────── /opencr_sim/joint_states (가상 pan 각도만큼 카메라 시야 회전) ────────────────────────────────────┘
```

| 구분 | 위치 |
|---|---|
| 시험 작업공간 | pa16 `~/test_ws` (저장소 밖) |
| `realsense` (인지, 팀) | `~/test_ws/src/realsense` — `origin/feature/perception` 사본 |
| `dynamixel` (제어, 팀) | `~/test_ws/src/dynamixel` → 저장소 `lv2_module5/ros2_ws/src/dynamixel` 링크 (`feature/control`) |
| `bringup` (launch·bag 도구) | `~/test_ws/src/bringup` — `origin/docs/documents` 사본 + `record_bag`·`analyze_bag` |
| `virtual_env` (가상 환경) | `~/test_ws/src/virtual_env` — `camera_sim`·`opencr_sim`·`gui_bridge`·`sim.launch.py` |
| 가상 환경용 설정 | `virtual_env/config/realsense_sim.yaml` (디버그 영상 10 Hz), `dynamixel_sim.yaml` (`serial_port: /tmp/opencr_sim`) |
| GUI 앱 (노트북) | `~/lv2_sim_viewer/viewer_app.py`, 프로그램 메뉴 "Lv2 추적 시뮬레이터" |
| 실행 로그 | `~/test_ws/logs/sim_launch.log` |

## 1. 빌드 (pa16, 코드를 바꿨을 때)

```bash
cd ~/test_ws
source /opt/ros/lyrical/setup.bash
colcon build --symlink-install --parallel-workers 1 --cmake-args -DBUILD_TESTING=OFF
```

| 상황 | 명령 |
|---|---|
| 제어(C++)만 | `colcon build --symlink-install --packages-select dynamixel` |
| Python 패키지만 | `colcon build --symlink-install --packages-select bringup virtual_env` |

## 2. 실행

### 2.1 GUI 앱으로 (노트북)

프로그램 메뉴에서 **Lv2 추적 시뮬레이터**를 실행하거나:

```bash
python3 ~/lv2_sim_viewer/viewer_app.py
```

| 영역 | 내용 |
|---|---|
| 인지 영상 | `perception_node` 디버그 영상 (검출 박스·중심·HSV) |
| 평면도 | 가상 pan 방향·화각(초록), pan 범위 135~225°(파란 점선), 목표 기둥(파란 사각), 하늘색 판(방해물) |
| 그래프 (20초) | ex 인지(초록) · ex 정답(회색) · pan 명령 rad/s(주황) · (pan−180°)/45°(파랑), LOST 구간 빨간 눈금 |
| 상태 | TRACKING / LOST / IDLE, 수치 |
| 목표 버튼 | 좌우 왕복 · 계단 이동 · 멈춤 · ◀5° · 중앙 · 5°▶ · 2초 가림 · 가림 · 보임 |
| 제어 파라미터 | Kp · direction 변경 후 [적용] |
| 가상 환경 | [시작] · [정지] · [다시 연결] — pa16의 `start_sim.sh`·`stop_sim.sh` 실행 |

앱은 `ssh pa16`으로 `gui_bridge`를 실행해 데이터를 받습니다. ROS 통신은 pa16 안에서만 일어납니다.

### 2.2 명령으로 (pa16)

```bash
~/test_ws/start_sim.sh                       # 백그라운드 시작 (인자는 sim.launch.py로 전달)
~/test_ws/start_sim.sh mode:=step distance:=0.8
~/test_ws/stop_sim.sh                        # 모두 종료
```

포그라운드로 실행할 때:

```bash
cd ~/test_ws && source install/setup.bash
ros2 launch virtual_env sim.launch.py        # mode:=sweep|step|static  distance:=0.5  pan_sign:=1|-1
```

- `pan_sign:=-1`: pan이 증가할 때 카메라가 왼쪽으로 도는 장착을 흉내냅니다. 이때 `direction`이 +1이면 목표에서 멀어지므로 −1로 바꿔야 합니다 (부호 확인 시험).
- `Parameter 'use_motor' is not supported` 같은 노란 경고는 realsense2_camera launch가 출력하는 것으로 동작에 영향이 없습니다.

## 3. 확인 (pa16 터미널)

```bash
cd ~/test_ws && source install/setup.bash
ros2 topic echo /target --qos-reliability best_effort --field point   # x = 좌우 오차, z = 0 이면 미검출
ros2 topic echo /virtual_env/truth --field point                       # x = 정답 ex
ros2 topic echo /tracking_status                                       # IDLE · TRACKING · LOST
ros2 topic echo /motor_cmd --field velocity                            # [pan, tilt] rad/s
ros2 topic echo /opencr_sim/joint_states --field position              # 가상 모터 각도 [rad] (π = 180°)
ros2 topic pub --once /virtual_env/command std_msgs/msg/String "{data: occlude_pulse}"   # 버튼과 같은 명령
ros2 param set /dynamixel_move_node kp 0.3
```

| 동작 | 기대 결과 |
|---|---|
| 좌우 왕복 | `TRACKING` 유지, 가상 pan이 목표를 따라감, ex가 ±0.4 이내 |
| 2초 가림 | 즉시 `LOST`, pan 명령 0, 가상 pan 정지 → 다시 보이면 3프레임 후 `TRACKING` |
| 하늘색 판만 화면에 | 미검출 (HSV 범위 밖) |
| 목표가 pan 범위 밖 | pan 135°/225°에서 `LIMIT pan`, 정지 |

## 4. 기록·분석 (rosbag2)

```bash
mkdir -p ~/test_ws/rec
ros2 run bringup record_bag success --out-dir ~/test_ws/rec --duration 20 \
  --topics /camera/camera/color/image_raw /target /motor_cmd /tracking_status /opencr_sim/joint_states /virtual_env/truth
ros2 run bringup analyze_bag ~/test_ws/rec/<RUN_ID> --csv ~/test_ws/rec/<RUN_ID>.csv
```

- 출력: `rec/<YYYYMMDD_HHMMSS>_<장면>/` (`*.db3` + `metadata.yaml`), `rec/<RUN_ID>.sha256`
- 토픽 탐색에 2~4초가 걸려 bag 기간은 `--duration`보다 짧습니다. 영상 포함 시 12초에 약 240MB입니다.
- 분석: 처리 FPS, 검출 비율, 수평 RMSE, 추적 구간 비율, 상태 전이, 소실·복구 시간, `/target` 공백

## 5. 안전 동작 시험

| 시험 | 명령 (pa16) | 기대 결과 |
|---|---|---|
| 인지 입력 중단 (TRACKING 중) | `pkill -f perception_node` | 0.5초 후 `LOST`, 로그 `/target 입력 중단 (타임아웃)` |
| 제어 명령 중단 | `pkill -f dynamixel_move_node` | 컨트롤러 로그 `/motor_cmd 0.50s 이상 미수신`, 가상 모터 속도 0 |
| 시리얼 명령 중단 | `pkill -f dynamixel_controller` | 가상 OpenCR 로그 `→ TIMEOUT`, 속도 0 |

시험 후 `~/test_ws/start_sim.sh`로 다시 시작합니다.

## 6. 실제 장비로 바꿀 때 (장비 프로필)

가상 환경 구성은 그대로 두고, 그 구성을 움직이는 값만 장비 프로필 한 파일에 모았습니다.

| 파일 (pa16 `~/test_ws/profiles/`) | 내용 |
|---|---|
| `sim.yaml` | 현재 가상 환경 값 (참고용). 이 파일로 생성한 결과 = 지금 가상 환경 설정 |
| `real.yaml` | 제로 베이스. 같은 항목이 모두 빈 칸이며, 항목마다 `[sim]` 값과 `[찾는 법]` 주석 |
| `profile_tool.py` | `check` 빈 칸·값·장치 점검 / `generate` 설정 생성 |
| `out/<이름>/` | 생성물: `dynamixel.yaml`, `opencr_pan_tilt/`(팀 .ino + 프로필 값 config.h), `launch.args` |

| 항목 묶음 | 내용 | 필요한 때 |
|---|---|---|
| `camera` | 모델·시리얼·해상도·영상 토픽 | 실제 카메라 실행 |
| `control` | direction·kp·speed_limit·deadband·타임아웃·복귀 프레임·주기 | 항상 |
| `opencr` | 시리얼 포트·baud·명령 타임아웃 | 모터 출력 |
| `dynamixel` | 모델·프로토콜·baud·속도 상한·pan/tilt ID·회전 범위 | 펌웨어 생성 |

```bash
nano ~/test_ws/profiles/real.yaml                                   # 빈 칸 채우기
python3 ~/test_ws/profiles/profile_tool.py check real --probe       # 빈 칸·장치 점검
python3 ~/test_ws/profiles/profile_tool.py generate real            # 설정 생성
~/test_ws/start_real.sh real                                        # 실제 카메라 + 인지 + 제어, 모터 출력 없음
~/test_ws/start_real.sh real --motor                                # 모터 출력 (펌웨어 업로드 후)
~/test_ws/stop_sim.sh                                               # 정지
```

- `start_real.sh`는 필요한 항목이 비어 있거나 장치 확인이 실패하면 실행하지 않습니다.
- 펌웨어: `out/real/opencr_pan_tilt/`를 노트북으로 복사해 Arduino IDE로 업로드합니다 (OpenCR 보드 패키지는 x86 전용). 팀 저장소의 `config.h`는 바뀌지 않습니다.
- `direction`은 실제 장착에서 목표가 오른쪽일 때 오차가 줄어드는 쪽으로 확인한 뒤 넣습니다.
