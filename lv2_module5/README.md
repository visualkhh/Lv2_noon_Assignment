# lv2_module5 — 실행·재현 가이드

<!-- 다른 팀원이 이 문서만 보고 실행·재현할 수 있도록 작성합니다. -->

## 1. 환경

| 항목 | 값 (테스트베드 기준) | 실기 (확정 시 갱신) |
|---|---|---|
| OS | Ubuntu 26.04 Resolute (Docker `ros:lyrical-ros-base-resolute`) | 라즈베리파이 Ubuntu Server 26.04 |
| ROS2 | Lyrical Luth (`ROS_DISTRO=lyrical`) | 합의 후 확정 (현재 테스트베드=Lyrical) |
| OpenCV | `libopencv-dev` + `python3-opencv` (이미지 내장) | 동일 |
| 카메라 (모델·해상도·설정 FPS) | 미정 — USB 카메라 연결 후 기입 | |
| 모터 (모델·ID·baud·프로토콜) | 미정 — OpenCR 연결 후 기입 | |
| 제어 통신 방식 | 미정 | |

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
├── config/             # HSV·면적·해상도·Kp·제한값 설정
├── recordings/         # bag 파일 또는 다운로드 링크·체크섬
└── results/            # (생성 예정) 원본 CSV·이미지·그래프
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

# 테스트베드 (컨테이너, 실기 없이): bringup(use_camera:=false) + 더미 카메라
ros2 launch fake_camera_bringup fake_camera_bringup.launch.py    # image_dir:=… period_s:=…
```

더미 카메라(`fake_camera`)는 `/ws/debug/input-images`의 숫자 이름 이미지(1.png, 3.png …)를
숫자 순서대로 0.1초에 1장 `/camera/camera/color/image_raw`·`camera_info`로 발행 (realsense2_camera와 같은 토픽).
모터 명령은 `dynamixel.yaml`의 `/dev/ttyACM0`로 나가고, 컨테이너에선 이게 가상 시리얼로 연결돼
호스트 `lv2_module5/debug/serial-out`에 쌓임.
같이 뜨는 `monitor_manager`는 이미지 토픽(카메라·마스크·debug_image)을 자동으로 찾아
`debug/topic/<토픽>/image.jpg`로 저장 → 통제실 images 패널.
모든 토픽의 마지막 메시지는 JSON `{"type", "data"}`로 `debug/topic/<토픽>/message`에 덮어씀 (rosx_introspection으로 런타임 파싱 →
토픽이 늘어도 재빌드 불필요, 긴 배열은 제외) → 통제실 status 패널.

> `period_s`는 0.5초(`target_timeout`)보다 짧게. 1초처럼 길면 장면마다 `/target` 타임아웃으로
> LOST로 떨어져 모터 명령이 계속 0 (TRACKING 상태에서만 움직임 명령이 나옴).

### 중앙통제실 (호스트 Mac)

```bash
cd docker/test-controller
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # 최초 1회
.venv/bin/python run-controller.py
```

웹캠 + 캡처 버튼(→ input-images), serial-out 실시간, 토픽 버튼 → echo 실시간.
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
> test-all        # 전체 (firmware→serial→bringup→fake_camera_bringup+logger) → ALL PASS면 push
> test-bringup    # 빌드 + 실기 bringup launch → 노드 4개 전부 뜨면 PASS
> test-fake_camera_bringup [초] [간격]  # 빌드 + 더미카메라→perception→/target→dynamixel→시리얼 → PASS면 정상 (간격 기본 0.1s)
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

| 파일 | 내용 | 주요 파라미터 |
|---|---|---|
| | | |

## 5. 실행

### 5.1 검출 노드만 실행 (모터 출력 없음)

```bash
```

### 5.2 전체 추적 실행

```bash
```

### 5.3 정지·종료

```bash
```

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
| | | | | |

## 7. 기록 (rosbag)

### 7.1 기록 명령

```bash
```

### 7.2 bag 목록

| 파일 | 장면 | 기간 | 토픽 | 용량 | 체크섬 | 기준 커밋 |
|---|---|---|---|---|---|---|
| | | | | | | |

## 8. 재현 (모터 출력 비활성)

### 8.1 입력 재처리

```bash
```

### 8.2 결과 재분석

```bash
```

## 9. 지표 계산

```bash
```

## 10. 문제 해결

| 증상 | 원인 | 조치 |
|---|---|---|
| | | |
