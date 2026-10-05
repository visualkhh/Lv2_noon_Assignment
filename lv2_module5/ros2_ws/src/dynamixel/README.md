# dynamixel — 추적 제어와 OpenCR 시리얼 브리지

이 패키지는 인지 담당자의 `realsense` 패키지가 발행하는 `/target`을 받아 Pan/Tilt의 **상대 이동량**을 계산한다. `dynamixel_move_node`와 `dynamixel_controller`는 별도 프로세스다. 카메라 영상 처리와 `+180°` 절대 위치 변환은 여기서 하지 않는다.

## 연결 계약

| 구간 | 타입·QoS | 데이터 |
| --- | --- | --- |
| `realsense/perception_node` → `/target` → `dynamixel_move_node` | `geometry_msgs/msg/PointStamped`, best effort·volatile·depth 1 | `x`: 오른쪽 + 정규화 오차, `y`: 아래쪽 + 정규화 오차, `z`: OpenCV contour 면적 비율. `z=0`은 미검출 |
| `dynamixel_move_node` → `/motor_cmd` → `dynamixel_controller` | `sensor_msgs/msg/JointState`, reliable·volatile·depth 1 | `name=[pan_joint, tilt_joint]`, `position=[pan_delta_rad, tilt_delta_rad]`; velocity/effort 없음 |
| controller → `/dev/opencr` → OpenCR | 115200 bps ASCII | `M,<pan_delta_deg>,<tilt_delta_deg>\n` (실제 newline 바이트) |
| `dynamixel_move_node` → `/tracking_status` | `std_msgs/msg/String`, reliable·transient local·depth 1 | 현재 FSM 상태 `IDLE`, `TRACKING`, `LOST` |
| OpenCR → XM430-W350-T | Protocol 2.0, 1,000,000 bps | pan ID **11**, tilt ID **12** |

인지 패키지는 `/image_raw`를 구독하고 `realsense.launch.py`에서 기본 `/camera/camera/color/image_raw`로 remap한다. `/target`은 검출·미검출 영상마다 발행하고, 카메라가 멈추면 발행하지 않는다. 제어 노드는 미검출 프레임의 x/y를 사용하지 않으며, `/target`이 끊겨도 타이머로 LOST에 전이한다.

## Pi에 빌드하기

실제 Pi 저장소에서 실행한다. 다음 경로는 현재 Pi의 저장소 경로다. ROS 배포판이 다르면 setup 경로를 맞춘다.

```bash
cd ~/git/Lv2_noon_Assignment/lv2_module5/ros2_ws
source /opt/ros/lyrical/setup.bash
colcon build --packages-select dynamixel
source install/setup.bash
ros2 pkg executables dynamixel
```

필요한 ROS 패키지는 `ament_cmake`, `rclcpp`, `geometry_msgs`, `sensor_msgs`, `std_msgs`, `launch`, `launch_ros`다. `realsense`도 함께 빌드하려면 [인지 패키지 README](../realsense/README.md)의 OpenCV·cv_bridge·yaml-cpp 의존성을 먼저 설치한다. 인지 패키지 코드는 그 디렉터리의 원본을 그대로 사용한다.

## Settings

Raspberry Pi 사용자 권한, OpenCR udev 규칙, `/dev/opencr` 설정은 [Settings](settings.md)를 참고한다.

다른 Raspberry Pi에서 OpenCR을 연결한 뒤, 실제 장치가 `/dev/ttyACM0`이면 아래 스크립트로 사용자 그룹·udev 규칙을 설정하고 `dynamixel`을 빌드할 수 있다. ROS 2와 colcon 및 패키지 의존성은 먼저 설치해야 한다. ROS 배포판이 `lyrical`이 아니면 `--ros-distro` 값을 지정한다. 스크립트는 자동으로 재부팅하지 않으므로 그룹 변경 후 로그아웃·로그인하고 OpenCR을 다시 연결한다.

```bash
./setup_pi.sh --device /dev/ttyACM0
```

현재 상태를 확인하려면 다음 명령을 실행한다. IDLE에서도 기본 1초 주기로 값이 반복해서 표시된다.

```bash
ros2 topic echo /tracking_status std_msgs/msg/String --qos-durability transient_local
```

## 모터 없이 제어 확인하기

터미널마다 `source /opt/ros/lyrical/setup.bash`와 `source install/setup.bash`를 실행한다.

1. 터미널 A: `ros2 run dynamixel dynamixel_move_node --ros-args --params-file install/dynamixel/share/dynamixel/config/dynamixel.yaml`
2. 터미널 B: `ros2 topic echo /motor_cmd --qos-reliability reliable`
3. 터미널 C: 아래 target을 한 번 발행한다. 첫 유효 target은 IDLE→TRACKING을 만들고, `x=0.5`, `y=-0.5`에 해당하는 음의 pan·음의 tilt delta가 나와야 한다.

```bash
ros2 topic pub --once /target geometry_msgs/msg/PointStamped \
  '{point: {x: 0.5, y: -0.5, z: 0.02}}' --qos-reliability best_effort
```

`z: 0.0`을 발행하면 명령이 없어야 한다. 마지막 유효 target 뒤 `lost_timeout` 이상 기다리면 LOST 로그가 나와야 한다. 다시 유효 target을 발행하면 TRACKING으로 복귀한다. Pi에서 자동 확인할 때는 `python3 test_contract.py`를 실행한다. 이 시험은 `/target`을 직접 발행하고 PTY 가상 시리얼을 사용해 `M,5.0,-5.0\n`을 검증하므로 모터가 움직이지 않는다.

## 카메라·OpenCR과 함께 실행하기

먼저 [인지 패키지 README](../realsense/README.md)에 따라 `realsense`와 `realsense2_camera`를 빌드·설치한다. OpenCR에는 [펌웨어 README](../../../firmware/README.md)에 따라 스케치를 올린다. 실제 모터 ID가 11·12인지, 전원과 장착 방향·가동 범위가 맞는지 확인한다.

```bash
cd ~/git/Lv2_noon_Assignment/lv2_module5/ros2_ws
source /opt/ros/lyrical/setup.bash
source install/setup.bash
ros2 launch realsense realsense.launch.py
```

다른 터미널에서 다음을 실행한다.

```bash
cd ~/git/Lv2_noon_Assignment/lv2_module5/ros2_ws
source /opt/ros/lyrical/setup.bash
source install/setup.bash
ros2 launch dynamixel dynamixel.launch.py
```

이 launch는 제어 노드와 controller를 모두 켠다. `/motor_cmd`가 오면 실제 OpenCR로 전송한다. `Ctrl+C`로 종료한다. 먼저 `ros2 topic info -v /target`, `ros2 topic echo /target --qos-reliability best_effort --field point`, `ros2 topic info -v /motor_cmd`로 연결·QoS·값을 확인한다.

## 설정과 부호

`config/dynamixel.yaml`을 편집한 뒤 다시 launch한다. 값의 단위는 아래와 같다.

| 파라미터 | 의미 |
| --- | --- |
| `lost_timeout` | 마지막 유효 target 이후 LOST까지 초 |
| `horizontal_deadband`, `vertical_deadband` | 정규화 오차의 무시 구간 |
| `pan_gain`, `tilt_gain` | 정규화 오차 1당 상대 radian. 부호를 음수로 바꾸면 해당 축 방향이 반대가 됨 |
| `max_pan_command`, `max_tilt_command` | 한 target 프레임의 최대 상대 radian |
| `serial_port`, `baud_rate` | OpenCR USB 포트(기본 `/dev/opencr`), 115200 bps |
| `status_publish_period` | `/tracking_status` 현재 상태 반복 발행 주기(초, 기본 1초) |

`/tracking_status`는 `reliable`, `transient_local` QoS로 IDLE/TRACKING/LOST 현재 상태를 발행한다. 시작 시와 상태 전이 시 즉시 발행하고, 이후 `status_publish_period`마다 현재 상태를 반복 발행한다.

`x/y` 오차 부호는 영상 좌표 기준이다. 실제 기구에서 대상 쪽으로 움직이는지 작은 gain과 좁은 가동 범위로 확인하고, 반대로 움직이면 해당 gain의 부호를 바꾼다. PID는 없다. controller는 관절 이름으로 값을 찾아 한 번만 radian→degree로 바꾼다. 이름 누락·중복, 크기 불일치, NaN/무한대는 전송하지 않고 로그에 남긴다.

현재 pan 장착 방향에서는 화면 오른쪽 대상(`x>0`)을 향한 pan delta가 음수여야 하므로 기본 `pan_gain`은 `-0.1`이다. tilt 방향 설정은 그대로 유지한다.


소스: `src/dynamixel_move_node.cpp`, `src/dynamixel_controller.cpp`. 실행 파일: `dynamixel_move_node`, `dynamixel_controller`.

## pan이 너무 빠를 때 속도 조정

이 제어 노드는 유효한 `/target` 프레임마다 `pan_gain × point.x`를 pan 상대 이동량(rad)으로 계산한다. 같은 방향의 대상이 연속으로 검출되면 상대 이동량이 계속 누적되므로, 한 프레임 명령이 작아도 움직임이 빠를 수 있다.

먼저 `src/dynamixel/config/dynamixel.yaml`에서 아래처럼 pan 관련 값만 낮춰 시험한다. 현재 장착 방향에 필요한 `pan_gain`의 음수 부호는 유지한다.

```yaml
pan_gain: -0.02
max_pan_command: 0.0174533  # 한 프레임 최대 약 1°
```

예를 들어 `point.x = 0.5`이면 한 프레임의 pan 명령은 `-0.01 rad`(약 `-0.57°`)이다. 현재 YAML의 `pan_gain: -0.03`에서는 같은 오차에 `-0.015 rad`(약 `-0.86°`)이다. 실제 속도는 검출 프레임 주기와 모터 동작에도 영향을 받으므로, 작은 값부터 시작해 장비에서 확인하며 조정한다.

설정 변경 후 실행 중인 `dynamixel.launch.py`를 재시작한다. `--symlink-install`로 빌드했다면 YAML만 수정할 때는 재빌드가 필요하지 않다. C++ 제어 코드를 변경했다면 `colcon build --packages-select dynamixel`을 다시 실행해야 한다. 위 값은 조정 예시이며 현재 기본 설정을 바꾼 것은 아니다.
