# lv2_module5 — 실행·재현 가이드

<!-- 다른 팀원이 이 문서만 보고 실행·재현할 수 있도록 작성합니다. -->

## 1. 환경

| 항목 | 값 (테스트베드 기준) | 실기 (확정 시 갱신) |
|---|---|---|
| OS | Ubuntu 26.04 Resolute (Docker `ros:lyrical-ros-base-resolute`) | 라즈베리파이 Ubuntu Server 26.04 |
| ROS2 | Lyrical Luth (`ROS_DISTRO=lyrical`) | 합의 후 확정 (현재 테스트베드=Lyrical) |
| OpenCV | `libopencv-dev` + `python3-opencv` (이미지 내장) | 동일 |
| 카메라 (모델·해상도·설정 FPS) | fake_camera (통제실 3D 시뮬레이션 영상, 640×480, 기본 30fps) | RealSense D435, 640×480 rgb8, 30fps ([perception_env_record.md](results/perception_env_record.md)) |
| 모터 (모델·ID·baud·프로토콜) | 가상 시리얼 (`/dev/ttyACM0` → PTY → `debug/serial-out`) | XM430-W350-T, ID 11 pan / 12 tilt, 1Mbps, Protocol 2.0 ([config.h](firmware/opencr_pan_tilt/config.h)) — TODO(제어): 실기 확인 |
| 제어 통신 방식 | USB 시리얼 `M,Δpan,Δtilt\n` [deg], 115200bps | 동일 |

> 실기 없이 학습·개발할 때는 이 문서의 Docker 명령만 따르면 됩니다.
> 실기 값이 확정되면 위 표의 오른쪽 열을 갱신합니다.

## 2. 폴더 구조

```
lv2_module5/
├── README.md           # 이 문서
├── report.md           # 최종 보고서
├── team.md             # 역할·Issue·PR·리뷰 기록
├── presentation.md     # 5분 시연 순서
├── ros2_ws/src/        # ROS2 패키지 (인지·제어 노드)
├── firmware/           # OpenCR 펌웨어
├── test-checklist.md   # 필수 시험·제출 체크리스트 (PDF 기준)
├── config/             # 사용한 설정 파일 목록 (패키지 안 설정으로 링크)
├── recordings/         # bag·영상 위치, 메타데이터, 재생 방법
└── results/            # 제출용 산출물
    ├── images/         #   장면별 원본·마스크·검출 이미지
    ├── logs/           #   원본 로그·CSV (실행 ID별)
    ├── plots/          #   그래프
    └── metrics.csv     #   지표 원본 (권장 열: run_id, time_s, frame_id, detected, ex, ey, area_ratio, state, command, command_unit)
```

> 마운트: `ros2_ws/src`→`/ws/src`, `firmware`→`/ws/firmware`,
> `config`→`/ws/config`(읽기전용), `results`→`/ws/results`(제출용 산출물),
> `debug`→`/ws/debug`(휘발성 디버그 로그, git 추적 안 함).
> `build/install/log`는 컨테이너 내부.

## 3. 설치와 빌드

전제: `docker/` 디렉토리에서 실행. Docker만 있으면 됨 (로컬에 ROS2 설치 불필요).

### 3.1 테스트베드 준비 (처음 1번)

```bash
cd docker
docker compose build
docker compose run --rm lyrical
```

컨테이너 안에서 환경 확인:

```bash
printenv ROS_DISTRO   # lyrical
lsb_release -a        # Ubuntu 26.04
source /opt/ros/lyrical/setup.bash
ros2 --help
```

### 3.2 ROS2 패키지 빌드

```bash
# 컨테이너 안, /ws 기준
rosdep install --from-paths src --ignore-src -r -y || true
colcon build --symlink-install
source install/setup.bash
```

`src`가 비어있으면 빌드할 게 없음 — 패키지 추가 후 다시 실행.

### bring-up (실행 / 테스트베드)

```bash
# 실기 (카메라·OpenCR 연결): 인지(realsense) + 제어(dynamixel) 전부
ros2 launch bringup bringup.launch.py              # use_camera:=false / use_motor:=false 로 끌 수 있음

# 테스트베드 (컨테이너, 실기 없이): bringup(use_camera:=false) + 더미 카메라 + monitor_manager
ros2 launch fake_camera_bringup fake_camera_bringup.launch.py    # period_s:=… (기본 0.033 = 30fps)
```

더미 카메라(`fake_camera`)는 통제실 3D 시뮬레이션이 그린 `/ws/debug/input-live/frame.png`를
30fps로 `/camera/camera/color/image_raw`·`camera_info`로 발행 (realsense2_camera와 같은 토픽).
실제로 발행한 장면은 `debug/output-images/fake-camera.png`에도 저장 → 통제실 화면과 같으면 정상.
모터 명령은 `dynamixel.yaml`의 `/dev/ttyACM0`로 나가고, 컨테이너에선 이게 가상 시리얼로 연결돼
호스트 `lv2_module5/debug/serial-out`에 쌓임.
같이 뜨는 `monitor_manager`는 이미지 토픽(카메라·마스크·debug_image)을 자동으로 찾아
`debug/topic/<토픽>/image.jpg`로 저장 → 통제실 images 패널.
모든 토픽의 마지막 메시지는 JSON `{"type", "data"}`로 `debug/topic/<토픽>/message`에 덮어씀 (rosx_introspection으로 런타임 파싱 →
토픽이 늘어도 재빌드 불필요, 긴 배열은 제외) → 통제실 status 패널.

**실기에 붙여 쓰기**: `monitor_manager`만 띄우면 통제실 status·images·JointState 다이얼이 그대로 동작 (echo 패널·serial 쌍은 비어 있음).
```bash
# Pi에서 (권장: 같은 컴퓨터라 영상이 네트워크를 안 탐) → debug_dir을 sshfs/rsync로 PC에 공유
ros2 run fake_camera_bringup monitor_manager --ros-args -p debug_dir:=$HOME/monitor
# 다른 PC에서 네트워크로 받을 때는 무압축 영상을 반드시 제외 (640x480 30fps ≈ 27MB/s)
ros2 run fake_camera_bringup monitor_manager --ros-args -p debug_dir:=/path/lv2_module5/debug \
  -p raw_images:=false -p exclude_topics:="[/camera/camera/color/camera_info]"
```
파라미터: `debug_dir`(debug 폴더 — 그 아래 `topic/<토픽>/`에 저장, 기본 `/ws/debug`), `period_s`(이미지 저장 간격, 기본 0.2), `raw_images`(false면 `sensor_msgs/Image` 구독 안 함), `exclude_topics`(이름으로 제외).

> `period_s`는 0.5초(`target_timeout`)보다 짧게. 길면 프레임마다 `/target` 타임아웃으로
> LOST로 떨어져 모터 명령이 계속 0 (TRACKING 상태에서만 움직임 명령이 나옴).

### 중앙통제실 (호스트 Mac)

```bash
cd docker/test-controller
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # 최초 1회
.venv/bin/python run-controller.py
```

**3D 시뮬레이션 (닫힌 루프)**: 기구 URDF(`docker/test-controller/pan_tilt.urdf`, pytransform3d로 TF 계산) +
360° 배경(`images/background/pano_*.jpg`, Poly Haven CC0) + 월드에 놓인 파란 기둥.
관절각 = serial-out `M,Δpan,Δtilt` 누적(펌웨어와 같은 계산) → 그 카메라 시점으로 렌더 → `debug/input-live/frame.png`
→ fake_camera(live) → perception → dynamixel → serial → 관절각… 기둥을 옮기면 카메라가 따라 돌아 가운데로 맞춘다.
컨테이너에서 `test-fake_camera_bringup 0` (기본 30fps). 통제실은 하나만 실행됨(잠금).
world 뷰: 클릭 = 상자 선택(노랑) · 드래그 = 이동 · Shift+드래그 = 높이 · Option(macOS)/Alt(Linux·Windows)+드래그 = 회전 · 오른쪽/Ctrl 드래그 = 시점 · 휠 = 줌
(회전은 슬라이더로도, [선택 색변경]으로 선택한 물체(기둥·장애물) 색 변경 — perception HSV 범위 안인지 표시).
장애물(회색 벽판) 기본 2개 + [장애물 추가]/[선택 삭제]. 기둥을 벽 뒤에 숨기면 미검출(z=0) → LOST, 다시 나오면 TRACKING.
[영상 송출] 토글 (처음엔 꺼짐 — 장면을 놓고 켜기): 끄면 `frame.png`를 지워 fake_camera가 발행을 멈춤(카메라 뽑힌 상황, 통제실을 닫아도 꺼짐).
끈 채로 launch → `/tracking_status` IDLE → 켜면 TRACKING → 끄면 0.5초 뒤 LOST.
그 밖에 status·모터 다이얼·serial-out·이미지·토픽 echo 실시간.
URDF 치수는 사진 기준 추정값 — 실측하면 origin·size 숫자만 바꾸면 됨.
컨테이너에서 띄우기·기록을 따로 켜 둠:
```bash
docker compose exec lyrical test-fake_camera_bringup 0   # 터미널 1: 띄우기 (Ctrl+C까지)
docker compose exec lyrical test-logger 0                # 터미널 2: 토픽 echo 기록 (통제실 토픽 패널용)
```

### 3.3 OpenCR 펌웨어 빌드·업로드

> FQBN: `OpenCR:OpenCR:OpenCR`. 스케치는 `firmware/`에 둠.
> 컴파일은 arm64 컨테이너에서도 됨 (시스템 툴체인 우회, 실측 성공).
> 단 업로더가 없어서 **업로드는 OpenCR USB 연결된 x86_64 PC에서**.

```bash
# 보드·라이브러리 확인 (컨테이너 안)
arduino-cli core list          # x86_64에서만 OpenCR 표시
arduino-cli lib list | grep Dynamixel

# 컴파일 (x86_64, 스케치 폴더에 .ino와 동명일 것)
arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR --library Dynamixel2Arduino ./firmware/opencr_pan_tilt

# 업로드 traffic 확인 (가상 시리얼 고정 — 쏜 바이트 수신 판정)
test-firmware   # details + compile + upload traffic → PASS면 정상

# 진짜 업로드 (OpenCR USB 연결된 x86_64 PC에서, 포트 확인 후 수동)
arduino-cli board list
arduino-cli upload -p /dev/ttyACM0 --fqbn OpenCR:OpenCR:OpenCR ./firmware/opencr_pan_tilt
```

**Raspberry Pi (Ubuntu arm64) / x86 PC에서 실물 업로드: `firmware/upload.sh`**

```bash
cd lv2_module5/firmware
./upload.sh --setup            # 최초 1회 (sudo): arduino-cli·OpenCR 코어(arm 우회)·Dynamixel2Arduino·업로더
./upload.sh                    # 컴파일 → .opencr 변환 → /dev/ttyACM0 업로드
./upload.sh /dev/ttyACM1 opencr_pan_tilt   # 포트·스케치 지정
```

> ARM용 공식 업로더(opencr_ld)가 없어서 ROBOTIS가 Pi용으로 주는 `opencr_ld_shell`(TurtleBot3 opencr_update)로 올림.
> `.bin`은 바로 못 올리고 `.opencr`로 변환 필요 (스크립트가 처리). 32비트 ARM 바이너리라 `libc6:armhf`도 setup이 설치.
> ROS의 `dynamixel_controller`가 포트를 잡고 있으면 거부 → 먼저 종료. 업로드가 계속 실패하면 SW2 누른 채 RESET(부트로더 모드).
> 검증: 새 Ubuntu 26.04 arm64 컨테이너에서 setup → 컴파일 → 변환 → 업로더가 포트로 부트로더 진입 명령 송신까지 확인 (실보드 업로드는 미검증).

> push/PR하면 CI(`firmware-compile`, x86_64)가 전 스케치 컴파일을
> 공식 툴체인으로 자동 검증. 로컬(arm64) 컴파일도 됨.
>
> 검사 한 방 (컨테이너 안):
> ```bash
> test-all        # 전체 (serial→firmware→bringup) → ALL PASS면 push. fake_camera_bringup은 따로
> test-bringup    # 빌드 + 실기 bringup launch → 노드 4개 + logger 기록 확인되면 PASS
> test-fake_camera_bringup [초] [간격]  # 빌드 + 가상 카메라→perception→/target → PASS (통제실 필요, 간격 기본 0.033s)
> test-firmware   # FQBN 유효 + 전 스케치 컴파일 → PASS면 정상
> test-serial     # 가상 시리얼 왕복 → PASS면 정상
> test-logger [초]  # 떠 있는 토픽 덤프만 (launch 안 함) → debug/topic/{토픽}/echo·info
> ```
> `test-fake_camera_bringup [초]`·`test-logger [초]` 모두 0이면 Ctrl+C까지.
> 띄우기(`test-fake_camera_bringup`, `bringup`)와 기록(`test-logger`)은 분리 —
> 실기 bringup을 띄워도 logger로 그대로 기록. CI(test-all)는 둘을 같이 돌려 덤프를 artifact로 남김.
> 진짜 업로드는 실물 보드가 필요해서 자동 검사 불가.
> CI는 업로드 전제조건(FQBN·`opencr_ld` 존재)까지만 검증하고,
> 업로드 본체는 실기 PC에서 수동으로.

하드웨어 없이 통신 로직만 검증할 때는 가상 시리얼 사용
(`socat` 이미지 내장, 루트 README 참고).

## 4. 설정 파일

목록과 링크: [config/README.md](config/README.md)

| 파일 | 내용 | 주요 파라미터 |
|---|---|---|
| `ros2_ws/src/realsense/config/realsense.yaml` | 인지 | HSV 범위 2개, 면적비, 종횡비·채움비, 영상 QoS, 디버그 영상 |
| `ros2_ws/src/dynamixel/config/dynamixel.yaml` | 제어 | `pan_gain` −0.03, `tilt_gain` 0.06, 데드밴드 0.05, `max_*_command` 5°, `lost_timeout` 0.5s, `/dev/ttyACM0` 115200 |
| `firmware/opencr_pan_tilt/config.h` | 장치 | ID 11/12, 1Mbps, 목표각 −180~179.9° |

## 5. 실행

### 5.1 검출 노드만 실행 (모터 출력 없음)

```bash
# 실기: 카메라 + 인지
ros2 launch realsense realsense.launch.py
# 인지 + 제어, 모터 출력 끔 (dynamixel_controller 안 띄움 → 시리얼로 안 나감)
ros2 launch bringup bringup.launch.py use_motor:=false
```

### 5.2 전체 추적 실행

```bash
# 실기 (Raspberry Pi, 카메라·OpenCR 연결)
ros2 launch bringup bringup.launch.py
# 테스트베드 (컨테이너): 통제실을 켜고 [영상 송출]을 켠 뒤
docker compose exec lyrical test-fake_camera_bringup 0
```

### 5.3 정지·종료

```bash
# launch 터미널에서 Ctrl+C (노드 전부 종료, 테스트 스크립트는 남은 노드도 정리)
# 실기에서 노드가 남았는지 확인
pgrep -fa "realsense|dynamixel"
```

> TODO(제어): 종료 시 모터가 어떤 상태로 멈추는지(마지막 목표각 유지) 실기 확인 기록

### 5.4 디버그 덤프 (가상 시리얼 ↔ 파일)

컨테이너 시작 시 entrypoint가 가상 시리얼 브릿지를 자동 구성함
(수동 `tee` 불필요). `debug/`는 git 추적 안 함:

| 경로 | 용도 |
|---|---|
| `/dev/ttyV0` | 앱이 여는 가상 시리얼 포트 (OpenCR 대체) |
| `debug/serial-out` | `/dev/ttyV0`에 들어온 데이터가 쌓이는 파일 (자동 append) |
| `debug/serial-in` | 여기다 쓰면 `/dev/ttyV0`으로 전송됨 (append 방식) |

```bash
# 컨테이너 안 (/ws 기준) — 앱 대신 손으로 쏴보기
echo "hello" >> debug/serial-in   # /dev/ttyV0 읽는 쪽에 전달됨

# 호스트에서 쌓인 수신 로그 확인
cat lv2_module5/debug/serial-out
```

> 시작할 때 두 파일이 자동 초기화됨. 주의: `/dev/ttyV1`은 브릿지
> 내부용이라 앱에서 열면 로그가 갈라지니 `/dev/ttyV0`만 사용할 것.
>
> 자가진단 (브릿지 bring-up 확인, 컨테이너 안):
> ```bash
> test-serial   # PASS 나오면 정상
> ```
> push/PR하면 CI도 동일 검사를 자동 실행.

## 6. 인터페이스

| 토픽 | 메시지 타입 | 방향 | QoS | 설명 |
|---|---|---|---|---|
| `/camera/camera/color/image_raw` | sensor_msgs/Image (rgb8) | camera → perception | reliable | 640×480 30fps |
| `/target` | geometry_msgs/PointStamped | perception → move_node | best-effort, depth 1 | x=ex, y=ey, z=면적비 (0=미검출), header=원본 영상 시각 |
| `/tracking_status` | std_msgs/String | move_node → (모니터) | reliable, transient_local | IDLE·TRACKING·LOST, 상태가 바뀔 때 발행 |
| `/motor_cmd` | sensor_msgs/JointState | move_node → controller | reliable | name [pan_joint, tilt_joint], position = Δ[rad] |
| `/perception_node/debug_image/compressed` · `mask/compressed` | sensor_msgs/CompressedImage | perception → (모니터) | reliable | 검출 표시·마스크 JPEG |
| 시리얼 | `M,<Δpan>,<Δtilt>\n` [deg] | controller → OpenCR | 115200bps | 펌웨어가 목표각 누적·범위 제한 |

상세 규약: [report.md 3.2](report.md#32-인터페이스-표)

## 7. 기록 (rosbag)

### 7.1 기록 명령

[recordings/README.md](recordings/README.md) 참고. TODO(통합): 실제 사용한 명령으로 확정

### 7.2 bag 목록

| 파일 | 장면 | 기간 | 토픽 | 용량 | 체크섬 | 기준 커밋 |
|---|---|---|---|---|---|---|
| | | | | | | |

## 8. 재현 (모터 출력 비활성)

### 8.1 입력 재처리

```bash
# 실제 모터 출력 끔. bag 영상만 검출기로 → /target_replay (저장된 /target과 섞지 않음)
ros2 launch realsense realsense.launch.py use_camera:=false target_topic:=/target_replay
ros2 bag play recordings/<RUN_ID> --topics /camera/camera/color/image_raw
```

### 8.2 결과 재분석

```bash
# TODO(통합): 저장된 /target·/tracking_status·/motor_cmd → 지표 재계산 스크립트
```

## 9. 지표 계산

산식은 [report.md 5.4](report.md#54-성능표) / [test-checklist.md](test-checklist.md) 참고.

```bash
# TODO(검증): results/metrics.csv → FPS·검출률·RMSE·복구율 계산 스크립트
```

## 10. 문제 해결

| 증상 | 원인 | 조치 |
|---|---|---|
| colcon 빌드가 매우 느리다가 `Killed signal terminated program cc1plus` | Docker VM 메모리 부족 (2GB) | Docker Desktop → Resources → Memory 6~8GB |
| `docker compose up`이 멈춘 것처럼 보임 | `-d` 없이 실행하면 컨테이너 bash에 붙음 | `docker compose up -d --build` 후 `docker compose exec lyrical bash` |
| 컨테이너를 새로 만든 뒤 스크립트·패키지가 옛날 것 | 이미지 재빌드 안 함 | `docker compose up -d --build` |
| `test-firmware` 업로드 바이트 불일치 | 다른 프로세스(dynamixel_controller)가 같은 시리얼에 씀 | `pgrep -fa dynamixel_controller` 후 종료 |
| 통제실 status가 빨강인데 노드는 정상 | 상태 토픽은 바뀔 때만 발행, 모터 명령은 오차가 있을 때만 발행 | 정상. 계속 발행되는 토픽(카메라·`/target`)으로 생존 확인 |
| fake_camera가 영상을 안 보냄 ("영상 송출 꺼짐") | 통제실 [영상 송출]이 꺼져 있거나 통제실 미실행 (처음엔 꺼짐) | 통제실을 켜고 [영상 송출] 체크 |
| 통제실을 하나 더 켜면 바로 종료 | 잠금: 두 개가 `frame.png`를 번갈아 쓰면 영상이 섞임 | 기존 창 사용 |
| 시뮬레이션에서 모터 명령이 항상 0 | 발행 간격이 `lost_timeout`(0.5s)보다 길거나 기둥이 데드밴드 안 | 간격 0.5초 미만, 기둥을 옆으로 이동 |

## 11. 실기(Raspberry Pi) 문제 해결

### 11.1 PC에서 Pi의 토픽이 안 보임 (`ros2 topic list`에 `/parameter_events`·`/rosout`만 나옴)

같은 네트워크에서도 아래 중 하나가 어긋나면 서로 발견하지 못한다. **양쪽(PC·Pi) 모두** 위에서부터 확인한다.

| 확인 | 명령 | 맞아야 하는 값 |
|---|---|---|
| ① 도메인 ID | `echo $ROS_DOMAIN_ID` | 양쪽 같은 값 (비어 있으면 **0**으로 동작) |
| ② RMW | `echo $RMW_IMPLEMENTATION` | 양쪽 같음 (둘 다 비어 있으면 기본 Fast DDS) |
| ③ 발견 범위 | `echo $ROS_AUTOMATIC_DISCOVERY_RANGE` | `SUBNET` 또는 비어 있음 (`LOCALHOST`·`OFF`면 안 보임) |
| ④ 같은 대역 | `ip -4 addr` | 같은 서브넷 (예: 둘 다 10.2.12.x) |
| ⑤ 방화벽 | `sudo ufw status` | 비활성, 또는 상대 대역 UDP 허용 |
| ⑥ 멀티캐스트 | 아래 시험 | Pi에서 `Received` 출력 |

```bash
# ① 도메인 맞추기 (양쪽, 계속 쓰려면 ~/.bashrc에 추가)
export ROS_DOMAIN_ID=9
ros2 daemon stop            # 예전 도메인으로 캐시된 목록 초기화
ros2 topic list

# ⑤ 방화벽: 같은 대역에서 오는 UDP 허용 (DDS 발견·통신은 UDP 7400번대 + 멀티캐스트)
sudo ufw allow from 10.2.12.0/24 proto udp

# ⑥ 멀티캐스트 시험 — Pi에서 받고 PC에서 보냄
ros2 multicast receive      # Pi
ros2 multicast send         # PC
```

⑥에서 `Received`가 안 뜨면 Wi-Fi(공유기)가 기기 간 멀티캐스트를 막고 있는 것이다 (학원·회사 Wi-Fi에서 흔함). 상대 IP를 직접 지정한다.

```bash
export ROS_STATIC_PEERS=<상대 IP>   # PC에선 Pi IP, Pi에선 PC IP (양쪽 모두)
ros2 daemon stop && ros2 topic list
```

> SSH로 Pi에 접속해서 Pi 안에서 `ros2 topic list`를 실행하면 네트워크 설정과 무관하게 보인다. 노드 동작 확인은 Pi 안에서, PC는 시각화(rviz·이미지 확인)용으로 쓸 때만 위 설정이 필요하다.

### 11.2 노드 시작 시 `OSError: [Errno 8] Exec format error`

`install/`이 다른 CPU용으로 빌드된 것이다 (예: CI x86_64 패키지를 Pi(aarch64)에서 실행). `start.sh`는 실행 전에 이를 검사해 멈춘다.

```bash
uname -m                                   # Pi = aarch64
./download-artifact.sh <RUN ID>            # 이 장치용(ros2_ws-deploy-aarch64.tar.gz) 받기
./start.sh --build                         # 또는 저장소에서 Pi에서 직접 빌드
```

### 11.3 그 밖의 실기 증상

| 증상 | 원인 | 조치 |
|---|---|---|
| `serial open /dev/opencr: No such file` | udev 링크 미설정 | `src/dynamixel/setup_pi.sh --device /dev/ttyACM0` |
| `Permission denied: /dev/ttyACM0` | 시리얼 권한 | `sudo usermod -aG dialout $USER` 후 재로그인 |
| `start.sh use_motor:=false`가 FAIL로 멈춤 | `dynamixel.launch.py`가 `use_motor`를 처리하지 않아 모터가 움직임 | 모터 전원(또는 OpenCR USB)을 분리하고 `use_motor:=false` 없이 실행 |
| 펌웨어 업로드 실패·응답 없음 | 포트 점유 또는 보드 미응답 | `start.sh` 등 ROS 노드 종료 후 재시도, 안 되면 OpenCR SW2 누른 채 RESET |
| bag 기록 중 영상 메시지 누락 | Pi SD카드·CPU 한계 (원본 영상 약 27MB/s) | `./bag-recording.sh -z`(압축) 또는 `-p control`, `<RUN_ID>.info.txt`의 Count 확인 |
| `download-artifact.sh`가 HTTP 401·403 | 토큰 없음·만료·권한 부족 | `./download-artifact.sh -h`의 토큰 안내 (Actions: Read-only) |

### 11.4 PC에서 실기(Pi) 토픽 모니터링 — `monitor_manager` + 통제실

`monitor_manager`(`fake_camera_bringup` 패키지)는 보이는 모든 토픽의 최신 메시지를 `<debug_dir>/topic/<토픽>/message`(JSON)·이미지로 저장한다.
PC에서 실행해 Pi 토픽을 `lv2_module5/debug/topic`에 저장하면, 통제실(`docker/test-controller/run-controller.py`)이 그 파일을 읽어 status·관절각을 보여준다.
(실기는 시리얼이 OpenCR로 바로 가서 serial-out이 비므로, 통제실은 `/motor_cmd`를 누적해 관절각을 추정한다.)

```
Pi:  start.sh → 토픽 발행 ──(같은 ROS_DOMAIN_ID, 11.1)──▶ PC: monitor_manager → lv2_module5/debug/topic/ → 통제실
```

**1. 준비 (PC, 최초 1회)**

```bash
sudo apt install ros-lyrical-rosx-introspection      # monitor_manager 의존성
cd lv2_module5/ros2_ws
colcon build --packages-select fake_camera_bringup
```

**2. Pi 토픽이 PC에서 보이는지 확인** — 안 보이면 11.1

```bash
export ROS_DOMAIN_ID=<Pi와 같은 값>
ros2 daemon stop && ros2 topic list                  # /target·/tracking_status 등이 보여야 함
```

**3. monitor_manager 노드만 실행 (PC)** — 기본 `debug_dir`은 컨테이너 경로(`/ws/debug`)라 호스트의 debug 폴더로 바꾼다

```bash
cd ~/workspaces/pa/source/Lv2_noon_Assignment/lv2_module5/ros2_ws
source install/setup.bash

# 기본
ros2 run fake_camera_bringup monitor_manager --ros-args -p debug_dir:=$PWD/../debug

# Pi 토픽을 Wi-Fi로 받을 때 (권장) — 원본 영상(약 27MB/s) 구독 안 함
ros2 run fake_camera_bringup monitor_manager --ros-args -p debug_dir:=$PWD/../debug -p raw_images:=false

# 스크립트로 (위와 같음: 저장 = monitor.sh 위치의 위 debug/, 원본 영상 제외) — 옵션은 ./monitor.sh -h
./monitor.sh -D 9                 # Pi와 같은 ROS_DOMAIN_ID
```

- 결과: `lv2_module5/debug/topic/<토픽>/message` (예: `debug/topic/target/message`). `Ctrl+C`로 종료.
- 폴더가 없으면 시작할 때 만든다. 못 만들면(권한 등) `[FATAL] 저장 폴더를 만들 수 없음`을 찍고 종료한다.
- 로그로 동작 확인:
  - 시작: `시작 | 저장: <경로> | ROS_DOMAIN_ID=<값> | ...`
  - 토픽 발견: `message 저장 시작: /target (...)`
  - 5초마다: `토픽 N개 구독 중, 최근 5초 저장 M건`
  - `구독할 토픽 없음` 경고가 계속 나오면 → 도메인·네트워크 문제 (11.1)
- `debug_dir`에는 **debug 폴더**를 준다. `topic/`은 노드가 붙인다 (`.../debug/topic`을 주면 `debug/topic/topic/...`이 됨).
- 예전 파라미터 `out_dir`은 없어졌다 — 주면 무시되고 기본값(`/ws/debug`)에 저장된다.
- `debug_dir`로 바뀐 뒤 처음이면 다시 빌드: `colcon build --packages-select fake_camera_bringup`

| 파라미터 | 기본값 | 설명 |
|---|---|---|
| `debug_dir` | `/ws/debug` | debug 폴더 — 그 아래 `topic/<토픽>/`에 저장. 호스트에선 `lv2_module5/debug` |
| `period_s` | `0.2` | 저장 주기(초) |
| `raw_images` | `true` | 원본 영상(`Image`)도 이미지 파일로 저장 |
| `exclude_topics` | `[]` | 저장하지 않을 토픽 (예: `-p exclude_topics:="['/camera/camera/color/image_raw']"`) |

**4. 통제실 실행 (PC, 다른 터미널)**

```bash
cd docker/test-controller && source .venv/bin/activate
python run-controller.py
```

- status: 토픽별 최신값·갱신 시각 (2초 넘게 안 바뀌면 빨강)
- motors: `/motor_cmd` 누적 관절각 (추정치 — 실제 모터 위치 아님)
- 통제실의 [영상 송출]은 fake_camera용이라 실기 모니터링에선 끈 채로 둔다

**주의**

- 원본 영상(`/camera/camera/color/image_raw`, 약 27MB/s)을 Wi-Fi로 받으면 네트워크가 막혀 Pi 쪽 추적에도 영향을 줄 수 있다. 영상은 `raw_images:=false` 또는 `exclude_topics`로 빼고, 압축 디버그 영상(`/perception_node/debug_image/compressed`)으로 확인한다.
- `debug/` 폴더가 root 소유라 쓰기 실패하면 `docker/README.md`의 권한 해결 참고 (`sudo chown -R $USER:$USER lv2_module5/debug`).
- PC에서 `ros2 run`한 노드도 Pi와 같은 도메인에 참여한다. 모니터링 중 PC에서 `/target`·`/motor_cmd`를 발행하는 노드(시뮬레이션·bag 재생 등)를 띄우면 **실기 모터가 움직일 수 있다.**
