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

### bring-up 스모크 (talker → listener → 가상 시리얼)

```bash
# 컨테이너 안 (/ws 기준), 터미널 A·B (up + exec 2번)
source install/setup.bash
ros2 run bringup_test talker     # A: /bringup_chatter 발행
ros2 run bringup_test listener   # B: 수신 → /dev/ttyV0 전송
# 또는 한 번에: ros2 launch bringup_test bringup.launch.py
```

수신 확인 2곳: B 터미널 `heard:` 로그 + 호스트 `lv2_module5/debug/serial-out`
파일. 포트 변경은 파라미터로 (`ros2 run bringup_test listener --ros-args -p serial_port:=/dev/ttyUSB0`).

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

> push/PR하면 CI(`firmware-compile`, x86_64)가 전 스케치 컴파일을
> 공식 툴체인으로 자동 검증. 로컬(arm64) 컴파일도 됨.
>
> 검사 한 방 (컨테이너 안):
> ```bash
> test-all        # 전체 (firmware→serial→bringup→run) → ALL PASS면 push
> test-bringup    # 빌드 + talker→listener→시리얼 → PASS면 정상
> test-firmware   # FQBN 유효 + 전 스케치 컴파일 → PASS면 정상
> test-serial     # 가상 시리얼 왕복 → PASS면 정상
> test-run [초]   # 빌드 + 실행 + 토픽 덤프 → debug/topic/{토픽}/echo·info
> ```
> `test-run 10` = 10초 수집 후 종료, `test-run 0` = Ctrl+C까지 무한 수집.
> `test-run`은 눈으로 보는 덤프용이지만 CI에서도 5초짜리로 돌려서
> launch 파일 검증을 겸함. echo는 계속 append됨.
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
