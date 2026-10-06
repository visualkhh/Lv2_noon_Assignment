#!/bin/bash
# 실기 실행: 환경 확인 → (필요하면 빌드) → bringup launch
#
#   ./start.sh                         # 카메라 + 인지 + 제어 + 모터 출력
#   ./start.sh use_motor:=false        # 모터 출력 없이 (launch 인자는 그대로 전달)
#   ./start.sh --build [launch 인자]   # 저장소에서 실행할 때 colcon build 후 실행
#
# 두 가지 위치에서 동작한다.
#   - 저장소 lv2_module5/ros2_ws/ : install/이 없거나 --build면 src/를 빌드
#   - CI 배포 패키지(ros2_ws-deploy) : 이미 빌드된 install/을 그대로 사용 (펌웨어 업로드는 firmware-upload.sh)
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-lyrical}"

build=0
if [ "${1:-}" = --build ]; then
  build=1
  shift
fi
case "${1:-}" in
  -h | --help) sed -n '2,10p' "$0"; exit 0 ;;
esac

# --- ROS 환경
ros_setup="/opt/ros/$ROS_DISTRO_NAME/setup.bash"
[ -f "$ros_setup" ] || { echo "FAIL: ROS 2 $ROS_DISTRO_NAME 없음 ($ros_setup)"; exit 1; }
# shellcheck disable=SC1090
source "$ros_setup"

# --- 빌드 (저장소에서 실행할 때만)
if [ "$build" = 1 ] || [ ! -f "$HERE/install/setup.bash" ]; then
  [ -d "$HERE/src" ] || { echo "FAIL: install/도 src/도 없음 ($HERE)"; exit 1; }
  echo "=== colcon build ($HERE)"
  (cd "$HERE" && colcon build --symlink-install)
fi
# shellcheck disable=SC1091
source "$HERE/install/setup.bash"

# --- 장치 확인 (경고만 — 모터·카메라 없이 실행하는 경우도 있음)
use_motor=true
use_camera=true
port=''
for arg in "$@"; do
  case "$arg" in
    use_motor:=false) use_motor=false ;;
    use_camera:=false) use_camera=false ;;
  esac
done

# 안전: use_motor:=false를 줬는데 설치된 dynamixel.launch.py가 그 인자를 처리하지 않으면
# dynamixel_controller가 그대로 떠서 실제 모터가 움직인다 → 시작하지 않는다.
if [ "$use_motor" = false ]; then
  dyn_launch="$(ros2 pkg prefix dynamixel)/share/dynamixel/launch/dynamixel.launch.py"
  if ! grep -q "use_motor" "$dyn_launch"; then
    echo "FAIL: use_motor:=false가 적용되지 않음 — $dyn_launch 가 use_motor 인자를 처리하지 않아"
    echo "      dynamixel_controller가 실행되고 모터가 움직인다."
    echo "      모터 없이 시험하려면 모터 전원(또는 OpenCR USB)을 분리한 뒤 use_motor:=false 없이 실행"
    exit 1
  fi
fi

if [ "$use_motor" = true ]; then
  # dynamixel_controller가 여는 포트 = dynamixel.yaml의 serial_port: /dev/opencr (따옴표·뒤 주석 허용)
  config="$(ros2 pkg prefix dynamixel)/share/dynamixel/config/dynamixel.yaml"
  port=$(sed -n "s/^[[:space:]]*serial_port:[[:space:]]*[\"']\{0,1\}\([^\"' #]*\).*/\1/p;T;q" "$config")
  [ -n "$port" ] || echo "WARN: $config 에서 serial_port를 못 찾음"
fi
if [ "$use_motor" = true ] && [ -n "$port" ]; then
  if [ ! -e "$port" ]; then
    echo "WARN: OpenCR 포트 없음: $port"
    echo "      USB 연결 확인, /dev/opencr가 없으면 src/dynamixel/setup_pi.sh --device /dev/ttyACM0 로 udev 설정"
  elif [ ! -w "$port" ]; then
    echo "WARN: $port 쓰기 권한 없음 → sudo usermod -aG dialout \$USER (재로그인)"
  else
    echo "ok: OpenCR $port"
  fi
fi

if [ "$use_camera" = true ]; then
  # grep -q는 pipefail에서 앞 명령이 SIGPIPE로 실패할 수 있어 출력을 먼저 받아 둔다
  if command -v rs-enumerate-devices >/dev/null; then
    devices=$(rs-enumerate-devices -s 2>/dev/null || true)
    if [[ $devices == *"Intel RealSense"* ]]; then
      echo "ok: RealSense 연결됨"
    else
      echo "WARN: RealSense를 찾지 못함 (USB 3 포트·udev 규칙 확인)"
    fi
  elif [[ $(lsusb 2>/dev/null || true) == *"8086:"* ]]; then
    echo "ok: Intel USB 장치 감지 (rs-enumerate-devices 없음)"
  else
    echo "WARN: RealSense를 찾지 못함"
  fi
fi

echo "=== ros2 launch bringup bringup.launch.py $*"
exec ros2 launch bringup bringup.launch.py "$@"
