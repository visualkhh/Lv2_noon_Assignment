#!/bin/bash
# 가상환경 + 웹 GUI: 빌드 → index.html을 http://localhost:8000 으로 제공 → sim.launch.py
#   GUI는 VS Code "Browser: Open Integrated Browser"에서 http://localhost:8000/index.html 로 연다
#
#   ./sim_gui.sh                       # 기기가 없으면 가상, RealSense·OpenCR를 꽂으면 자동으로 실기 전환
#   ./sim_gui.sh use_devices:=false    # 기기 자동 연결 끄고 가상환경만
# 최초 1회: sudo apt install ros-jazzy-rosbridge-suite
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
GUI="$HERE/../index.html"
HTTP_PORT="${HTTP_PORT:-8000}"

# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
ros2 pkg prefix rosbridge_server >/dev/null 2>&1 ||
  echo "WARN: rosbridge_server 없음 → GUI가 연결되지 않음 (sudo apt install ros-$ROS_DISTRO_NAME-rosbridge-suite)"

(cd "$HERE" && colcon build --symlink-install --packages-select cognitive_control)
# shellcheck disable=SC1091
source "$HERE/install/setup.bash"

# GUI는 http로 제공 (VS Code Integrated Browser는 file:// 대신 http 주소로 연다)
python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 --directory "$(dirname "$GUI")" >/dev/null 2>&1 &
HTTP_PID=$!
trap 'kill $HTTP_PID 2>/dev/null' EXIT
echo "=== GUI: http://localhost:$HTTP_PORT/index.html"
echo "    VS Code: Ctrl+Shift+P → Browser: Open Integrated Browser → 위 주소 입력"

ros2 launch cognitive_control sim.launch.py "$@"
