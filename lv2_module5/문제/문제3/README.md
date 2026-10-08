# 문제 3 — 객체 중심 기반 추적 제어

## 구현 내용

- ✅ 1. 추적을 끈 상태에서 제한된 범위의 작은 명령으로 모터 방향을 확인합니다.
- ✅ 2. 오른쪽 목표에 대한 명령이 실제 영상의 오차를 줄이는지 확인하여 부호를 정합니다.
- ✅ 3. P 제어부터 구현합니다. 속도형은 `command = clamp(direction × Kp × ex, -speed_limit, +speed_limit)`를 사용할 수 있습니다.
- ✅ 4. direction은 장착에 따라 +1 또는 -1이며 Kp·속도 명령의 단위를 적습니다. 속도를 위치로 적분한다면 실제 경과 시간 dt를 사용합니다.
- ✅ 5. 속도 상한·회전 범위·중심 데드밴드를 설정합니다. 범위 끝에서 바깥 방향으로 계속 명령하지 않습니다.
- ✅ 6. Kp 두 값을 같은 대상·거리·해상도·동작 순서로 비교합니다. 작은 값부터 시작하고 다른 조건은 유지합니다.
- ✅ 7. 오차·명령·상태를 시간과 함께 저장하고 비교 그래프를 만듭니다.

## 결과물과 보고서

- ✅ 두 Kp·제한값·시험 조건, 설정별 CSV·오차 그래프, 실제 추적 영상을 제출합니다. 최종 설정 선택 근거와 반응 속도·흔들림의 차이를 설명합니다. 모터 위치를 측정하지 않았다면 명령을 실제 위치처럼 표시하지 않습니다.

    - 설정별 CSV: [CSV](/lv2_module5/results/metrics.csv)
    - 그래프:

![그래프](/lv2_module5/assets/실험그래프.png)

<video src="../../assets/추적.mp4" controls width="600"></video>

영상: [추적.mp4](../../assets/추적.mp4)

## pan축 P 이득 비교 시험 조건 (실험 전 확정)

이 시험은 수평 pan축만 비교한다. `pan_gain = direction × Kp`이며 현재 장착 방향의
`direction`은 -1이다. 따라서 A의 Kp 크기는 0.015, B는 0.030이다.
두 설정 파일은 [`kp_pan_a.yaml`](../../ros2_ws/src/dynamixel/config/kp_pan_a.yaml)과
[`kp_pan_b.yaml`](../../ros2_ws/src/dynamixel/config/kp_pan_b.yaml)이다.

| 항목 | A | B | 기록 시 의미 |
| --- | ---: | ---: | --- |
| `pan_gain` | -0.015 | -0.030 | 정규화 수평 오차 1당 상대 명령 rad |
| `tilt_gain` | 0 | 0 | 수평 1축 비교 중 tilt 명령 없음 |
| `horizontal_deadband` | 0.05 | 0.05 | `abs(ex) <= 0.05`이면 pan 명령 없음 |
| `vertical_deadband` | 0.05 | 0.05 | tilt 명령은 이득 0으로 비활성화 |
| `max_pan_command` | 0.0174533 rad | 0.0174533 rad | 한 프레임당 최대 약 1°, 속도 상한은 아님 |
| `max_tilt_command` | 0.0174533 rad | 0.0174533 rad | tilt를 다시 켤 때 적용되는 한 프레임 상한 |
| `lost_timeout` | 0.5 s | 0.5 s | 마지막 유효 목표 후 LOST까지 |
| 명령 주기 | 유효 `/target` 프레임마다 | 동일 | 고정 제어 주기가 아니므로 실제 FPS 기록 |
| 카메라 | 424×240, 설정 30 FPS | 동일 | 실기 기본 `color_profile`; 실제 수신 FPS도 bag으로 확인 |
| 목표 | 파란 사각 기둥 30×30×60 mm | 동일 | 같은 물체를 계속 사용 |

예를 들어 `ex=+0.4`이면 A는 pan `-0.006 rad`(약 -0.34°), B는
`-0.012 rad`(약 -0.69°)를 한 프레임에 명령한다. 두 값 모두 1° 상한 안이다.

실기 배치: 카메라 앞 **0.8 m**에 목표를 둘 수 있는 좌·중·우 표시를 한다.
중앙을 기준으로 좌우 각각 **15 cm** 위치에 표시하고, 목표 높이·조명·배경을 고정한다.
각 회차 시작 전 중앙 목표를 영상 중앙에 맞추고 2초간 안정시킨다.
이후 **왼쪽 3초 → 중앙 3초 → 오른쪽 3초 → 중앙 3초** 순서로 옮긴다.
A/B를 각각 3회 기록하고, 매회 같은 표시와 순서를 사용한다. 시험 순서는
`A_1, B_1, A_2, B_2, A_3, B_3`으로 한다. 한 회차마다 제어 노드를 재시작하며
사용한 설정 파일과 bag 실행 ID를 같이 기록한다.

시험 전에 실제 기구의 간섭 없는 회전 범위와 초기 위치를 확인한다. 현재 펌웨어의
`-180°~+179.9°`는 실제 기구의 안전 범위를 보장하지 않는다. 위의 1° 제한도
**프레임당 상대 명령 상한**이라 반복 명령의 누적 이동 범위를 제한하지 않는다.
먼저 모터 출력 없이 아래 절차로 토픽과 명령 방향을 확인하고, 실기에서는 작은
수동 명령으로 `abs(ex)`가 줄어드는 방향인지 확인한 뒤 본 시험을 진행한다.

### Pi에서 이미 빌드된 노드로 실행 (재빌드 없음)

각 터미널에서 `cd` 후 ROS 환경을 읽는다. 기존 `bringup` 또는 `dynamixel.launch.py`는
종료해 중복 제어 노드가 없도록 한다. 새 A/B YAML은 시험 조건 보관용이며, 실행할 때는
설치된 노드에 아래 파라미터를 직접 넘기므로 설치 파일을 바꾸거나 재빌드하지 않는다.

```bash
cd ~/git/Lv2_noon_Assignment/lv2_module5/ros2_ws  # Pi의 실제 저장소 경로에 맞게 변경
source /opt/ros/lyrical/setup.bash
source install/setup.bash
```

1. 터미널 1: `ros2 launch realsense realsense.launch.py color_profile:=424x240x30`
2. 터미널 2: 아래 A 명령으로 이동 노드 실행. **controller를 켜지 않은 상태**에서
   `/target`, `/motor_cmd`의 부호와 크기를 확인한다.

```bash
ros2 run dynamixel dynamixel_move_node --ros-args \
  -p pan_gain:=-0.015 -p tilt_gain:=0.0 \
  -p horizontal_deadband:=0.05 -p vertical_deadband:=0.05 \
  -p max_pan_command:=0.0174533 -p max_tilt_command:=0.0174533 \
  -p lost_timeout:=0.5 -p status_publish_period:=1.0
```

3. 실기 확인 후 터미널 3에서 OpenCR 전송 노드만 실행한다:
   `ros2 run dynamixel dynamixel_controller --ros-args -p serial_port:=/dev/opencr -p baud_rate:=115200`.
4. 터미널 4에서 `./bag-recording.sh -p all -d 18 A_1`로 기록한다. 토픽 구독 완료 후
   중앙 2초와 좌·중·우·중 각 3초의 이동을 시작한다.
5. B 회차는 **터미널 2의 이동 노드만 종료**하고 위 명령의 `pan_gain`만 `-0.03`으로
   바꿔 재실행한다. 기록 이름은 `B_1`이다. 이후 `A_2, B_2, A_3, B_3`로 반복한다.

위 명령의 `-p` 값은 [A](../../ros2_ws/src/dynamixel/config/kp_pan_a.yaml)·
[B](../../ros2_ws/src/dynamixel/config/kp_pan_b.yaml) 파일과 동일하다.

bag의 `/target`, `/motor_cmd`, `/tracking_status`, 원본 영상 메시지 수와 실제
기록 기간을 매회 확인한다. `results/metrics.csv`는 현재 헤더만 있는 서식이며
bag 기록만으로 자동 채워지지 않는다. 추출 시 미검출 프레임의 `ex`는 0오차가
아니므로 빈값, 발행되지 않은 `command`도 빈값으로 둔다.
