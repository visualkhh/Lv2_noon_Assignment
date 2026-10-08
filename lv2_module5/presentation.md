# 최종 보고서 — 비전 객체 추적 시스템

## 시스템 구성
### 구조도

```mermaid
flowchart LR
    CAM["📷 Intel RealSense"]

    subgraph RPI["Raspberry Pi · Ubuntu 26.04 LTS · ROS2 Lyrical"]
        direction LR
        subgraph PER["인지 (Perception)"]
            direction TB
            RS["realsense2_camera<br/>Intel 공식 ROS2 래퍼<br/>컬러 영상 발행"]:::ext
            PN["PerceptionNode<br/>HSV · Contour 검출<br/>중심 오차 계산"]
            RS -- "/camera/camera/color/image_raw<br/>sensor_msgs/Image<br/>QoS: 발행 reliable → 구독 best-effort" --> PN
        end

        subgraph CTL["제어 (Control)"]
            direction TB
            MV["DynamixelMoveNode<br/>상태 관리 · P 제어<br/>IDLE · TRACKING · LOST"]
            DC["DynamixelController<br/>명령 → 시리얼 변환<br/>입력 타임아웃 정지"]
            MV -- "/motor_cmd<br/>sensor_msgs/JointState<br/>QoS: reliable · volatile · depth 1" --> DC
        end

        PN -- "/target<br/>geometry_msgs/PointStamped<br/>QoS: best-effort · volatile · depth 1" --> MV
        BAG[("ROS2 Bag<br/>/camera/camera/color/image_raw<br/>/target · /motor_cmd · /tracking_status")]
    end

    subgraph OCR["OpenCR"]
        FW["OpenCR Firmware<br/>명령 수신 · 통신 타임아웃 정지"]
    end

    MOT["⚙️ Dynamixel × 2<br/>pan · tilt"]

    CAM -- "USB" --> RS
    DC -- "USB 시리얼" --> FW
    FW -- "TTL" --> MOT
    MOT -. "카메라 방향 변화" .-> CAM

    RS -. "record" .-> BAG
    PN -. "record" .-> BAG
    MV -. "/tracking_status<br/>std_msgs/String<br/>QoS: reliable · transient_local · depth 1" .-> BAG

    classDef ext stroke-dasharray: 5 5
```

점선 테두리는 외부 패키지(직접 구현하지 않는 노드)입니다.

### 노드

| 구분 | 노드 | 역할 | 구독 | 발행 |
|---|---|---|---|---|
| 인지 | `realsense2_camera` (외부 패키지) | RealSense 컬러 영상 발행 (Intel 공식 ROS2 래퍼) | — (USB 카메라) | `/camera/camera/color/image_raw` |
| 인지 | `PerceptionNode` | HSV·Contour 검출, 정규화 중심 오차·면적비 계산 | `/camera/camera/color/image_raw` | `/target` |
| 제어 | `DynamixelMoveNode` | 상태 전이(IDLE·TRACKING·LOST), P 제어, 속도·범위·데드밴드 제한 | `/target` | `/motor_cmd`, `/tracking_status` |
| 제어 | `DynamixelController` | 모터 2개(pan·tilt) 명령을 OpenCR 시리얼 프로토콜로 변환·전송, 입력 타임아웃 시 정지 명령 | `/motor_cmd` | — (USB 시리얼) |
| 펌웨어 | OpenCR Firmware | 시리얼 명령 수신 → Dynamixel 구동, 통신 타임아웃 시 정지 | — (USB 시리얼) | — (Dynamixel) |

#### 토픽 정의

| 토픽 | 이름 근거 | 메시지 타입 | 발행 → 구독 | QoS |
|---|---|---|---|---|
| `/camera/camera/color/image_raw` | realsense2_camera 기본 | `sensor_msgs/msg/Image` | realsense2_camera → PerceptionNode | 발행: reliable · volatile (드라이버 기본) / 구독: best-effort · volatile · depth 1 |
| `/target` | 과제 규약 | `geometry_msgs/msg/PointStamped` | PerceptionNode → DynamixelMoveNode | best-effort · volatile · depth 1 |
| `/motor_cmd` | 팀 정의 | `sensor_msgs/msg/JointState` | DynamixelMoveNode → DynamixelController | reliable · volatile · depth 1 |
| `/tracking_status` | 과제 예시 | `std_msgs/msg/String` | DynamixelMoveNode → (모니터링·bag) | reliable · transient_local · depth 1 |

- PerceptionNode는 코드에서 `/image_raw`를 구독하고, launch에서 `/image_raw:=/camera/camera/color/image_raw`로 remap합니다 (`image_topic` 인자).
- DynamixelController → OpenCR 구간은 토픽이 아닌 USB 시리얼(`MotorSerialCommand`, 커스텀)입니다.
- 타임아웃: 마지막으로 신선한 입력을 받은 뒤 0.5초가 지나면 정지합니다. DynamixelMoveNode는 `/target`, DynamixelController는 `/motor_cmd`, OpenCR은 시리얼 명령 기준으로 각각 판단합니다.

##### QoS 범례

| 정책 | 값 | 의미 | 사용 토픽 |
|---|---|---|---|
| Reliability | `best-effort` | 유실된 메시지를 재전송하지 않음. 지연이 작고 최신 데이터가 중요한 센서 데이터에 사용 | 카메라 영상 구독(PerceptionNode), `/target` |
| Reliability | `reliable` | 유실 시 재전송하여 전달을 보장 | 카메라 영상 발행(realsense2_camera 기본), `/motor_cmd`, `/tracking_status` |
| Durability | `volatile` | 구독 이후에 발행된 메시지만 받음 (기본값) | 카메라 영상, `/target`, `/motor_cmd` |
| Durability | `transient_local` | 발행자가 마지막 메시지를 보관하여, 나중에 접속한 구독자도 즉시 받음 | `/tracking_status` |
| History | `keep_last` · `depth N` | 최근 N개만 큐에 보관. `depth 1`은 오래된 메시지를 버리고 최신 것만 유지 | 전체 |

- 호환성: 발행 측이 `best-effort`이면 구독 측이 `reliable`일 때 연결되지 않습니다. 발행 측이 `volatile`이면 구독 측이 `transient_local`일 때 연결되지 않습니다. 구독 측은 발행 측과 같거나 더 약한 정책을 사용합니다.
- `/tracking_status`는 상태가 바뀔 때마다, 그리고 1Hz 주기로 발행합니다. `transient_local`이므로 `ros2 topic echo`나 bag 기록을 나중에 시작해도 현재 상태를 바로 받습니다.


#### 메시지 구조

- 🟦 **ROS2 제공**: ROS2(Lyrical)에 포함된 표준 메시지입니다. 별도 정의 없이 사용합니다.
- 🟧 **커스텀**: 팀이 직접 정의하는 메시지·프로토콜입니다.
- 모든 ROS2 토픽은 ROS2 제공 메시지를 사용하므로 커스텀 메시지 패키지는 두지 않습니다. 커스텀은 OpenCR 시리얼 프로토콜뿐입니다.

##### 사용 메시지 타입 목록

| 메시지 타입 | 구분 | 패키지 | 사용 위치 |
|---|---|---|---|
| `sensor_msgs/msg/Image` | 🟦 ROS2 제공 | `sensor_msgs` | `/camera/camera/color/image_raw` |
| `geometry_msgs/msg/PointStamped` | 🟦 ROS2 제공 | `geometry_msgs` | `/target` |
| `geometry_msgs/msg/Point` | 🟦 ROS2 제공 | `geometry_msgs` | `PointStamped.point` |
| `sensor_msgs/msg/JointState` | 🟦 ROS2 제공 | `sensor_msgs` | `/motor_cmd` |
| `std_msgs/msg/String` | 🟦 ROS2 제공 | `std_msgs` | `/tracking_status` |
| `std_msgs/msg/Header` | 🟦 ROS2 제공 | `std_msgs` | `Image`·`PointStamped`·`JointState`의 `header` |
| `builtin_interfaces/msg/Time` | 🟦 ROS2 제공 | `builtin_interfaces` | `Header.stamp` |
| `MotorSerialCommand` | 🟧 커스텀 | `firmware/` (ROS2 메시지 아님) | DynamixelController → OpenCR USB 시리얼 |

##### `sensor_msgs/msg/Image` — 🟦 ROS2 제공

```
std_msgs/Header header    # stamp: realsense2_camera가 넣는 프레임 시각 (촬영 시각 기준 여부는 검증 후 기록)
                          # frame_id: "camera_color_optical_frame"
uint32  height            # 영상 높이 [px]
uint32  width             # 영상 너비 [px]
string  encoding          # "rgb8" (realsense2_camera 컬러 기본) → PerceptionNode에서 BGR로 변환
uint8   is_bigendian
uint32  step              # 한 행의 바이트 수 (width × 3)
uint8[] data              # 픽셀 데이터
```

##### `geometry_msgs/msg/PointStamped` — 🟦 ROS2 제공

```
std_msgs/Header header    # stamp: 원본 영상 시각 유지 (입력 영상의 stamp 복사)
geometry_msgs/Point point # 아래 Point 참고
```

> 이 과제의 목표 정보 전달 규약이며, 일반적인 3차원 위치로 해석하지 않습니다.
> 정상 영상에서 미검출이면 `z = 0`으로 발행합니다. 발행 중단(토픽 침묵)은 미검출과 구분하여 타임아웃으로 처리합니다.

##### `geometry_msgs/msg/Point` — 🟦 ROS2 제공

```
float64 x                 # ex = (cx − W/2) / (W/2), −1 ~ +1, 오른쪽 +
float64 y                 # ey = (cy − H/2) / (H/2), −1 ~ +1, 아래쪽 +
float64 z                 # 면적비 = contour_area / (W × H), 0 = 미검출 (이때 x·y는 사용하지 않음)
```

##### `sensor_msgs/msg/JointState` — 🟦 ROS2 제공

```
std_msgs/Header header    # stamp: 명령 생성 시각
string[]  name            # ["pan", "tilt"]
float64[] position        # 사용하지 않음 (빈 배열)
float64[] velocity        # 각 축 속도 명령 [rad/s], name과 같은 순서, 0 = 정지
float64[] effort          # 사용하지 않음 (빈 배열)
```

> 원래 관절 상태 보고용 타입이지만, 이 프로젝트에서는 모터 속도 명령 용도로 사용합니다.
> 기본 구현(수평 1축)에서는 `tilt` 속도를 항상 0으로 보냅니다.

##### `std_msgs/msg/String` — 🟦 ROS2 제공

```
string data               # "IDLE" | "TRACKING" | "LOST"
```

##### `std_msgs/msg/Header` — 🟦 ROS2 제공

```
builtin_interfaces/Time stamp   # 아래 Time 참고
string frame_id                 # 좌표계 이름 (예: "camera_color_optical_frame")
```

##### `builtin_interfaces/msg/Time` — 🟦 ROS2 제공

```
int32  sec                # 초
uint32 nanosec            # 나노초
```
