# feature/integration 작업 요약

- 브랜치: `feature/integration` (기준 `main` `eeca856`, 현재 `f721dbc`)
- 기간: 2026-10-05 ~ 2026-10-08, 커밋 76개
- 변경: 파일 61개, +2868 / −2

## 한눈에 보기

RealSense 카메라와 OpenCR이 **없어도** 인지 → 제어 → 모터 명령 루프 전체를 가상환경에서 돌려 볼 수 있게 했습니다. 기기를 USB에 꽂으면 **자동으로 실기로 전환**됩니다. 결과는 웹 GUI로 확인하고, 실행 장면은 rosbag으로 녹화해 팀원과 공유합니다.

```
virtual_world ──image_raw──▶ perception ──/target──▶ tracker ──/motor_cmd──▶ opencr_bridge ──serial──▶ OpenCR
   (RealSense가 꽂히면 device_manager가                          │
    realsense2_camera를 띄우고 가상 영상은 멈춤)                  └──▶ virtual_world (가상 pan/tilt 누적 → 닫힌 루프)

모든 토픽 ──rosbridge(ws://localhost:9090)──▶ index.html (웹 GUI)
```

## 1. ROS 2 패키지 `cognitive_control`

경로: `lv2_module5/ros2_ws/src/cognitive_control/`

| 노드 | 역할 | 주요 토픽 |
|---|---|---|
| `virtual_world` | 실제 카메라가 없을 때만 가상 영상을 발행합니다. 파란 사각 목표가 움직이고, 20초마다 3초씩 숨겨서 LOST 상황을 만듭니다. `/motor_cmd`를 누적해 가상 카메라 방향을 바꿉니다. | 발행 `/camera/camera/color/image_raw`, `/camera_source`, `/joint_states`, `/virtual_target` |
| `perception` | HSV 마스크와 가장 큰 연결 요소로 파란 목표를 찾습니다. 실기 `perception_node`와 같은 토픽 이름과 형식을 씁니다. | 발행 `/target` (x, y = 정규화 오차, z = 면적 비율, 미검출이면 0), 디버그 및 마스크 jpeg |
| `tracker` | 실기 `dynamixel_move_node`와 같은 규칙과 기본값을 씁니다. 데드밴드, 게인, 최대 명령(5°)을 적용하고 상태를 IDLE → TRACKING ↔ LOST로 바꿉니다. | 구독 `/target`, 발행 `/motor_cmd`, `/tracking_status` |
| `device_manager` | `/sys/bus/usb`를 2초마다 확인합니다. Intel RealSense가 꽂히면 `realsense2_camera`를 실행하고, 뽑히면 종료합니다. | — |
| `opencr_bridge` | `/dev/opencr` 또는 `/dev/ttyACM*`를 자동으로 연결하고, `/motor_cmd`를 `M,<Δpan>,<Δtilt>\n`(115200bps) 형식으로 전송합니다. **안전장치:** 카메라 소스가 `realsense`일 때만 전송하므로, 가상 영상을 보고 낸 명령으로 실제 모터가 움직이지 않습니다. | 발행 `/opencr_status`, `/joint_states` |

- launch: `ros2 launch cognitive_control sim.launch.py`
  - 인자 `use_sim`, `use_tracker`, `use_devices`, `port`(rosbridge, 기본 9090)
  - 제어를 팀의 다른 노드가 맡을 때는 `use_tracker:=false`로 실행합니다.
- 테스트: ament 기본 린트(copyright, flake8, pep257, xmllint)와 mypy를 추가했습니다.
- 설정: `lv2_module5/config/params.yaml`에 color_detector와 tracker 파라미터를 담았습니다. `enable_motor: false`가 기본입니다.

## 2. 웹 GUI (`lv2_module5/index.html`, `ros2_ws/sim_gui.sh`)

- roslib으로 rosbridge에 붙어서 디버그 영상, 마스크, `/target`, 추적 상태, 모터 명령, 카메라 소스, OpenCR 상태를 한 화면에 보여 줍니다.
- three.js 3D 뷰로 pan/tilt 자세(`/joint_states`)와 가상 목표 위치(`/virtual_target`)를 표시합니다.
- `./sim_gui.sh`를 실행하면 http://localhost:8000/index.html 웹 서버가 뜨고, 이어서 `run_and_record.sh`로 실행과 녹화를 한 번에 진행합니다.
- 다른 PC의 rosbridge에 붙으려면 주소 뒤에 `?ws=ws://<IP>:9090`을 붙입니다.

## 3. 녹화와 기기 연동 도구 (`lv2_module5/recordings/`)

이 폴더만 있으면 어떤 기기에서든 동작합니다. ROS 배포판(lyrical, kilted, jazzy, humble, 소스 빌드)은 자동으로 탐지합니다.

| 스크립트 | 용도 |
|---|---|
| `connect.sh [상대 주소]` | 기기 점검, ROS 탐지, 워크스페이스 빌드, VS Code 경로 연결, 상대 기기 연결을 차례로 하고 결과를 `.link.env`에 저장합니다. `--local`, `--docker` 옵션을 지원합니다. |
| `find_domain.py` | 상대 기기가 노드를 띄운 `ROS_DOMAIN_ID`를 자동으로 찾습니다(멀티캐스트 감청 후 유니캐스트로 확인). |
| `env.sh` | 공통 환경입니다. 기본은 로컬 모드이고, 연동 모드(`--link`)에서는 지정한 상대하고만 통신합니다. RMW는 `rmw_fastrtps_cpp`로 통일했습니다. |
| `run_and_record.sh` | 빌드, launch, 녹화, 압축과 체크섬, README 목록 등록을 한 번에 합니다. |
| `record_peer.sh`, `record_agumon.sh` | 라즈베리파이(`agumon.local`) 같은 상대 기기의 토픽을 녹화만 합니다. 노드는 띄우지 않습니다. |
| `record_scene.sh` | 이 PC에서 따로 띄운 노드를 녹화합니다. |
| `play_bag.sh` | 체크섬을 확인한 뒤 압축을 풀어 재생합니다. `LINK=1`이면 상대 쪽으로 재생하며, `/motor_cmd`는 기본으로 제외합니다. |
| `pack_bag.sh`, `register_bag.sh` | `.tar.gz`와 `SHA256SUMS`를 만들고, `metadata.yaml`을 읽어 README 목록에 자동으로 등록합니다. |
| `share_zip.sh` | 팀원 공유용 zip을 만듭니다(스크립트, 문서, `.tar.gz` 포함). |

**설계 원칙**
- 기본은 로컬 모드(`ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST`)입니다. 그래서 가상 실행이나 재생 중의 `/motor_cmd`가 실수로 로봇에 가지 않습니다.
- 연동 모드는 `ROS_STATIC_PEERS`로 지정한 상대하고만 통신하므로, 같은 Wi-Fi를 쓰는 다른 팀 노드와 섞이지 않습니다.
- bag은 `mcap` 형식이며, jazzy와 lyrical이 서로 읽을 수 있는 것을 확인했습니다(2026-10-07).

**녹화 결과 (`SHA256SUMS`에 등록된 17개)**
- 가상환경 `sim_*` 8개 (2026-10-06)
- 장면 녹화 `scene1`, `scene5`~`scene8` 5개
- 라즈베리파이 실기 `agumon_*` 4개 (2026-10-06 ~ 07)
- `/rosout`만 녹화된 빈 bag 7개는 `bags/_empty/`로 옮겨 목록에서 제외했습니다.

## 4. 기타

- **OpenCR 펌웨어** (`lv2_module5/firmware/opencr_update/`): burger, waffle, manipulation 바이너리와 업로드 도구(`update.sh`, `update.bat`)를 추가했습니다.
- **개발 환경**: `.vscode/c_cpp_properties.json`과 `settings.json`을 추가했습니다. `connect.sh`가 기기마다 만드는 ROS 경로 링크를 참조합니다. `.gitignore`에는 build, install, log, IntelliSense DB를 추가했습니다.
- **문서**: `order_20261005.md`는 노트북에서 조원 브랜치(`feature/control_perception`)를 Docker로 실행하는 절차입니다.

## 5. 머지 전 확인할 점

- **`lv2_module5/recordings.zip` (약 47MB)이 커밋되어 있습니다.** `recordings/.gitignore`는 bag을 Git에 올리지 않는 정책인데, 이 zip은 그 폴더 밖에 있어서 걸러지지 않았습니다. bag 원본이 포함되어 있으므로 커밋에서 빼고 외부 링크로 공유하는 것을 권장합니다.
- **펌웨어 파일이 중복**되어 있습니다. `firmware/opencr_update/`와 `firmware/opencr_update/opencr_update/`에 같은 파일이 있고, `opencr_update.tar.bz2`도 두 곳에 있습니다.
- **커밋 메시지가 대부분 한 단어**(`.vscode`, `SH`, `bag`, `zip` 등)입니다. 머지할 때 squash하거나 PR 본문에 이 요약을 함께 적는 것을 권장합니다.
- README 녹화 목록의 `다운로드:` 링크가 아직 `(업로드 후 기입)` 상태입니다.
