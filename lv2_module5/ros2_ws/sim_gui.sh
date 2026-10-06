#!/bin/bash
# 가상환경 + 웹 GUI: 빌드 → sim.launch.py (virtual_world + tracker + rosbridge) → 브라우저로 index.html
#
#   ./sim_gui.sh                                   # 장비 없이 가상환경
#   ./sim_gui.sh use_sim:=false use_tracker:=false # 실기 노드가 따로 돌 때 GUI 연결만
# 최초 1회: sudo apt install ros-jazzy-rosbridge-suite
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
GUI="$HERE/../index.html"

# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
ros2 pkg prefix rosbridge_server >/dev/null 2>&1 ||
  echo "WARN: rosbridge_server 없음 → GUI가 연결되지 않음 (sudo apt install ros-$ROS_DISTRO_NAME-rosbridge-suite)"

(cd "$HERE" && colcon build --symlink-install --packages-select cognitive_control)
# shellcheck disable=SC1091
source "$HERE/install/setup.bash"

# launch가 rosbridge를 띄울 시간을 두고 브라우저를 연다
(sleep 3; xdg-open "$GUI" >/dev/null 2>&1 || echo "브라우저로 열기: $GUI") &
exec ros2 launch cognitive_control sim.launch.py "$@"
