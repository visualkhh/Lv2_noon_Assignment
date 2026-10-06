#!/bin/bash
# 가상환경 + 웹 GUI: 빌드 → index.html을 http://localhost:8000 으로 제공 → sim.launch.py
#   GUI는 VS Code "Browser: Open Integrated Browser"에서 http://localhost:8000/index.html 로 연다
#   실행하는 동안 자동 녹화 → Ctrl+C로 끝내면 recordings/에 압축·SHA256SUMS·README [목록] 등록
#
#   ./sim_gui.sh                       # 기기가 없으면 가상, RealSense·OpenCR를 꽂으면 자동으로 실기 전환
#   ./sim_gui.sh use_devices:=false    # 기기 자동 연결 끄고 가상환경만
#   NO_RECORD=1 ./sim_gui.sh           # 녹화 안 함
#   RECORD_RAW=1 ./sim_gui.sh          # 원본 영상(image_raw, 약 13MB/s)까지 녹화
# 최초 1회: sudo apt install ros-jazzy-rosbridge-suite
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
GUI="$HERE/../index.html"
REC_DIR="$(cd "$HERE/../recordings" && pwd)"
HTTP_PORT="${HTTP_PORT:-8000}"
REC_TOPICS=(/target /motor_cmd /tracking_status /camera_source /opencr_status /joint_states /virtual_target
            /perception_node/debug_image/compressed)
[ "${RECORD_RAW:-0}" = 1 ] && REC_TOPICS+=(/camera/camera/color/image_raw)

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
REC_PID=''
cleanup() {
  kill "$HTTP_PID" 2>/dev/null || true
  [ -n "$REC_PID" ] && kill -TERM -- -"$REC_PID" 2>/dev/null || true
}
trap cleanup EXIT
echo "=== GUI: http://localhost:$HTTP_PORT/index.html"
echo "    VS Code: Ctrl+Shift+P → Browser: Open Integrated Browser → 위 주소 입력"

# --- 녹화 (별도 프로세스 그룹: launch가 끝난 뒤 따로 멈춰 bag을 정상 마무리)
if [ "${NO_RECORD:-0}" != 1 ]; then
  NAME="sim_$(date +%Y%m%d_%H%M%S)"
  echo "=== 녹화: recordings/$NAME (${REC_TOPICS[*]})"
  (cd "$REC_DIR" && exec setsid ros2 bag record -o "$NAME" "${REC_TOPICS[@]}" \
    >"${TMPDIR:-/tmp}/$NAME.record.log" 2>&1) &
  REC_PID=$!
fi

# Ctrl+C는 launch만 멈추고 이 스크립트는 녹화 정리까지 계속
trap ':' INT
ros2 launch cognitive_control sim.launch.py "$@" || true

[ -z "$REC_PID" ] && exit 0
echo "=== 녹화 종료: $NAME"
kill -INT -- -"$REC_PID" 2>/dev/null || true
for _ in $(seq 20); do kill -0 "$REC_PID" 2>/dev/null || break; sleep 0.5; done
kill -0 "$REC_PID" 2>/dev/null && kill -TERM -- -"$REC_PID" 2>/dev/null
wait "$REC_PID" 2>/dev/null || true
REC_PID=''

# 장면 설명: 녹화 중 카메라 출처·OpenCR 연결 여부 (/camera_source, /opencr_status)
DESC=$(python3 - "$REC_DIR/$NAME" <<'PY' 2>/dev/null || echo "sim_gui 자동 녹화"
import sys
from rclpy.serialization import deserialize_message
import rosbag2_py
from std_msgs.msg import String
reader = rosbag2_py.SequentialReader()
reader.open(rosbag2_py.StorageOptions(uri=sys.argv[1]), rosbag2_py.ConverterOptions('', ''))
seen = {'/camera_source': [], '/opencr_status': []}
while reader.has_next():
    topic, data, _ = reader.read_next()
    if topic in seen:
        v = deserialize_message(data, String).data.split(' ')[0]
        if v not in seen[topic]:
            seen[topic].append(v)
cam = '→'.join(seen['/camera_source']) or '-'
opencr = '→'.join(seen['/opencr_status']) or '-'
print(f'sim_gui 자동 녹화 (카메라: {cam}, OpenCR: {opencr})')
PY
)
"$REC_DIR/register_bag.sh" "$NAME" "$DESC"
