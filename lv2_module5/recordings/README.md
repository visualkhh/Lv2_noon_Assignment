# recordings — bag·영상 위치와 메타데이터

기록·패키징 스크립트: [scripts/](./scripts/)

PDF 문제 5: 대표 **성공 장면**과 **소실·복귀 장면**을 각각 10~30초 수준으로 기록하고,
토픽·메시지 수·기간·해상도·설정·기준 커밋과 파일명·크기·해시·다운로드·재생 방법을 남깁니다.

> 큰 bag·영상은 git에 올리지 않고 허용된 저장 위치에 둔 뒤 링크합니다. 평가자가 접근 가능한지 확인합니다.
> 개인 PC 경로만 적지 않습니다.

## 1. 기록 대상 토픽

| 토픽 | 타입 | 비고 |
|---|---|---|
| `/camera/camera/color/image_raw` | sensor_msgs/Image | 입력 재처리용 (용량 큼) |
| `/target` | geometry_msgs/PointStamped | 원본 검출 결과 |
| `/tracking_status` | std_msgs/String | 상태 |
| `/motor_cmd` | sensor_msgs/JointState | 제어 명령 (position = Δrad) |
| `/camera/camera/color/camera_info` | sensor_msgs/CameraInfo | 해상도·내부 파라미터 |

시리얼 로그는 같은 실행 ID로 `results/logs/<RUN_ID>_serial.log`에 저장합니다.

## 2. 기록 명령

```bash

RUN_ID=$(date +%Y%m%d_%H%M%S)_success
ros2 bag record -o recordings/$RUN_ID \
  /camera/camera/color/image_raw /camera/camera/color/camera_info /target /tracking_status /motor_cmd
ros2 bag info recordings/$RUN_ID          # 토픽·메시지 수·기간 → 아래 표
sha256sum recordings/$RUN_ID/*.mcap        # 저장 형식에 맞게 (db3/mcap)
```


## 3. 재생·재현 (실제 모터 출력 끔)

```bash
# 입력 재처리: bag 영상만 검출기로 → /target_replay (저장된 /target과 섞지 않음)
ros2 launch realsense realsense.launch.py use_camera:=false target_topic:=/target_replay
ros2 bag play recordings/<RUN_ID> --topics /camera/camera/color/image_raw   # 
```


## 4. bag 파일 링크

1. 정상 추적 및 소실 복귀 rosbag -> [noon팀 노션 -> 참고자료 모음 -> bag로그 파일](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0)
2. 로그 리플레이 및 /target_replay 발행 rosbag -> [noon팀 노션 -> 참고자료 모음 -> bag로그 파일](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef5148025ad7ec7baa931cd24)