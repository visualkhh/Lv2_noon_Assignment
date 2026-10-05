# Lv2_noon_Assignment — 비전 객체 추적 시스템

카메라로 단일 색상 목표를 검출하고, 화면 중심 오차를 기반으로 다이나믹셀을 제어하여 카메라가 목표를 추적하도록 만드는 팀 프로젝트입니다.
목표 소실·통신 단절 시의 안전 정지와 복귀, 그리고 ROS2 bag 기반 재현까지 하나의 시스템으로 검증합니다.

```
카메라 영상 → HSV·Contour 검출 → 중심 오차 → P 추적 제어 → OpenCR → 다이나믹셀 → 카메라 방향 변화 → 새 영상의 오차 확인
```

## 프로젝트 개요

| 구분 | 내용 |
|---|---|
| 범위 | 모듈 ⑤ · 32~36강 · 20시간 |
| 과제 구성 | 5문제 · 성취도 41~45 |
| 기본 환경 | 라즈베리파이 Ubuntu Server 26.04 · ROS2 **Lyrical** · OpenCV · SSH (과제 안내는 Humble로 표기 — Humble은 Ubuntu 22.04 전용이라 26.04에서는 Lyrical 사용) |
| 기본 장비 | USB 카메라(RealSense D435) · OpenCR · 다이나믹셀 XM430 ×2 · 전원 · 고정 브래킷 |
| 기본 구현 | 단일 색상 목표 1개 · 수평 1축 추적 · 소실·복귀 · 기록·재현 |
| 제출 위치 | `lv2_module5/` (README.md · report.md · team.md · presentation.md) |
| 제출 태그 | `lv2-module5-submit` |

## 팀 구성

| 이름 | 역할 | 담당 |
|---|---|---|
| 김현하 | 팀장 | 테크 리드, 문서 통합 |
| 정구영 | 팀원 | 통합 (ROS2 인터페이스·노드 연결·bag 재현) |
| 심규진 | 팀원 | 제어 (OpenCR·다이나믹셀·P 제어·안전 정지) |
| 문태영 | 팀원 | 인지 (HSV·Contour 검출·중심 오차 계산) |
| 공통 | 전원 | 성능·안전 검증, 코드 리뷰, 보고서 |

> 각 팀원은 병합된 PR 1개 이상, 다른 팀원의 PR에 남긴 구체적인 리뷰 1개 이상을 `team.md`에 연결합니다.

## 문제별 진행 계획

| 단계 | 내용 | 권장 시간 | 완료 기준 |
|---|---|---|---|
| 문제 1 | HSV·Contour 검출 파이프라인 | 4시간 | 저장소·PR 규칙 설정, 대상·시험 조건 확정, 검출 완성 |
| 문제 2 | 인지·제어 노드 연결 (`/target`) | 4시간 | 인터페이스 합의, 모터 출력 없이 데이터 전달 검증 |
| 문제 3 | 객체 중심 기반 추적 제어 | 4시간 | 실제 추적, Kp 두 설정 × 3회 비교 |
| 문제 4 | 성능 측정과 목표 소실 복구 | 4시간 | 소실·통신 끊김 정지와 복귀, 성능 측정 |
| 문제 5 | bag 재현과 팀 협업 | 4시간 | bag 재현, 다른 팀원 실행 확인, 보고서·시연·제출 |

> ⚠️ TODO(팀장): 제출 저장소는 **SpartaPA 조직의 `Lv2_팀명_과제`** 여야 합니다 (현재 개인 계정 `visualkhh/Lv2_noon_Assignment`).
> 각 팀원 GitHub ID를 위 표에 추가합니다.

## 저장소 구조

```
Lv2_noon_Assignment/
├── README.md               # 이 문서 (프로젝트·팀 소개)
├── CONTRIBUTING.md         # Git 협업 규칙
├── documents/              # 과제 안내 자료
└── lv2_module5/
    ├── README.md           # 실행·재현 방법
    ├── report.md           # 최종 보고서
    ├── team.md             # 역할·Issue·PR·리뷰 기록
    ├── presentation.md     # 5분 시연 순서와 핵심 결과
    ├── test-checklist.md   # 필수 시험·제출 체크리스트
    ├── ros2_ws/src/        # ROS2 패키지 (realsense 인지 · dynamixel 제어 · bringup · fake_camera_bringup)
    ├── firmware/           # OpenCR 펌웨어
    ├── config/             # HSV·면적·해상도·Kp·제한값 설정
    ├── results/            # 원본 CSV·이미지·그래프·시험 결과표
    └── recordings/         # bag 파일 또는 다운로드 링크·체크섬
```

## 협업 규칙

브랜치 전략, 커밋 메시지 작성법, PR·리뷰 절차는 [CONTRIBUTING.md](CONTRIBUTING.md)를 따릅니다.

## 테스트베드 (Docker · 5분 완성)

> 베이스: `ros:lyrical-ros-base-resolute` = Ubuntu 26.04 + ROS 2 Lyrical.
> Mac에서도 동작 (x86_64/arm64 멀티아키). 실기(라즈베리파이·OpenCR) 없이
> ROS2 빌드·노드 연결까지 검증하는 용도.

### 1. 준비물

- Docker + Docker Compose (Docker Desktop 포함)
- 이 저장소 (`develop` 기준 최신)

> 아래 `docker compose` 명령은 전부 **`docker/` 디렉토리 안에서** 실행합니다.
> (compose 파일이 `docker/docker-compose.yml`에 있어서 그럼)

### 2. 이미지 빌드 (처음 1번)

```bash
cd docker
docker compose build
```

> 이미지 내장: `ros-dev-tools`·`colcon`·OpenCV·데모 노드·
> `realsense2_camera`(4.58)·`cv_bridge`·`image_transport`·
> `socat`(가상 시리얼)·`arduino-cli`(1.5)·`Dynamixel2Arduino`(0.8).
> 인지 패키지의 `exec_depend(realsense2_camera)` 충족용.
>
> ⚠️ OpenCR 보드 코어: ROBOTIS 툴체인에 aarch64 빌드가 없어서,
> arm64(Mac·라즈베리파이) 컨테이너는 보드 패키지 수동 설치 +
> 시스템 arm-none-eabi-gcc 우회로 **컴파일 가능** (실측 성공).
> 단 업로더가 없어서 **업로드는 x86_64 실기 PC에서**.

### 3. 컨테이너 진입

```bash
# docker/ 안에서
docker compose run --rm lyrical
# 컨테이너 안에서:
# printenv ROS_DISTRO   # lyrical 나와야 정상
# lsb_release -a        # Ubuntu 26.04 (resolute) 확인
```

### 4. ROS2 스모크 테스트 (talker → listener)

`run`을 2번 하면 컨테이너가 2개(격리)라 안 통함.
같은 컨테이너를 띄우고 `exec`으로 2번 들어가야 함:

```bash
# docker/ 안에서
# 1. 컨테이너 1개 띄우기 (백그라운드)
docker compose up -d

# 2. 터미널 A, B 각각 실행 (같은 컨테이너에 2번 접속)
docker compose exec lyrical bash
```

```bash
# 터미널 A (exec 안)
ros2 run demo_nodes_cpp talker
```

```bash
# 터미널 B (exec 안)
ros2 run demo_nodes_py listener
```

`talker`가 발행하고 `listener`가 수신하면 DDS·ROS2 환경 정상.

```bash
# docker/ 안에서. 끝나면 컨테이너 내리기
docker compose down
```

### 5. 워크스페이스 빌드

소스는 `lv2_module5/ros2_ws/src`에 두고, 컨테이너에서 빌드:

```bash
# 컨테이너 안에서 (/ws 기준)
rosdep install --from-paths src --ignore-src -r -y || true
colcon build --symlink-install
source install/setup.bash
```

> `src`가 비어있으면(`.gitkeep`만) 빌드는 스킵됨. CI도 동일하게 동작.

### 6. 자주 쓰는 명령 (docker/ 안에서)

| 목적 | 명령 |
|---|---|
| 1회용 진입 (빠른 확인) | `docker compose run --rm lyrical` |
| 2터미널 작업용 (유지) | `docker compose up -d` → `docker compose exec lyrical bash` (A·B 각각) → `docker compose down` |
| 이미지 다시 빌드 | `docker compose build --no-cache` |
| ROS 환경 수동 로드 | `source /opt/ros/lyrical/setup.bash` |
| 빌드 결과 로드 | `source /ws/install/setup.bash` |

### 7. CI

`develop`·`main`에 push/PR하면 `.github/workflows/ci.yml`이 자동 실행:

1. `docker-build` — 테스트베드 이미지 빌드 + talker 스모크 테스트
2. `colcon-build` — `ros:lyrical-ros-base-resolute` 컨테이너에서 `colcon build`

빨간불이면 PR 머지 금지. 로그 보고 고친 뒤 다시 push.

## 작업 시작법 (팀원용)

```bash
git fetch origin
git checkout develop
git pull origin develop
git checkout -b chore/작업명   # 예: chore/docker-testbed-lyrical
# 작업 후
git push -u origin chore/작업명
# → GitHub에서 develop으로 PR → 팀원 1명 리뷰 승인 후 머지
```

각자 `lv2_module5/team.md`에 본인 Issue·PR·리뷰 링크를 기록합니다.
실행·재현 방법은 [`lv2_module5/README.md`](lv2_module5/README.md)를 따릅니다.
