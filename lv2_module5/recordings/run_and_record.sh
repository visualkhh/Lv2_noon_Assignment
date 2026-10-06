#!/usr/bin/env bash
# 사용법: ./run_and_record.sh [이름] [launch 인자...]
#   ./run_and_record.sh                          ← sim_YYYYmmdd_HHMMSS 이름으로, 가상환경 실행 후 녹화
#   ./run_and_record.sh scene5                   ← 이름 지정
#   ./run_and_record.sh scene5 use_tracker:=false ← launch 인자는 sim.launch.py로 그대로 전달
#   - 현재 브랜치의 cognitive_control을 빌드하고 sim.launch.py(virtual_world + tracker)를 백그라운드로 실행
#   - 토픽이 뜨면 record_scene.sh로 녹화, Ctrl+C로 녹화 종료 → 압축·README 등록 → launch 종료
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
TOPICS=(/camera/camera/color/image_raw /target /motor_cmd /tracking_status)
WAIT_SEC=60                         # 노드 기동 대기 최대 시간
# ================

set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WS="$(cd "$HERE/../ros2_ws" && pwd)"

# 이름: ':='가 없는 첫 인자 > sim_YYYYmmdd_HHMMSS (sim_gui.sh와 같은 규칙)
if [ -n "$1" ] && [[ "$1" != *:=* ]]; then
  NAME="${1%/}"; shift
else
  NAME="sim_$(date +%Y%m%d_%H%M%S)"
fi
[ -e "$HERE/$NAME" ] && { echo "$NAME 폴더가 이미 있음 — 다른 이름을 주세요"; exit 1; }

# --- ROS 환경 + 빌드
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
(cd "$WS" && colcon build --symlink-install --packages-select cognitive_control)
# shellcheck disable=SC1091
source "$WS/install/setup.bash"

# --- 가상환경 실행 (별도 프로세스 그룹: 녹화 중 Ctrl+C가 launch까지 죽이지 않도록)
LOG="${TMPDIR:-/tmp}/$NAME.launch.log"
echo "=== ros2 launch cognitive_control sim.launch.py $* (로그: $LOG)"
setsid ros2 launch cognitive_control sim.launch.py "$@" >"$LOG" 2>&1 &
SIM=$!

stop_sim() {
  kill -0 "$SIM" 2>/dev/null || return 0
  echo "=== 가상환경 종료"
  kill -INT -- -"$SIM" 2>/dev/null || true
  for _ in $(seq 20); do kill -0 "$SIM" 2>/dev/null || return 0; sleep 0.5; done
  kill -TERM -- -"$SIM" 2>/dev/null || true
}
trap stop_sim EXIT

# --- 토픽 대기
echo "=== 토픽 대기 (최대 ${WAIT_SEC}s): ${TOPICS[*]}"
for ((t = 0; t < WAIT_SEC; t += 2)); do
  if ! kill -0 "$SIM" 2>/dev/null; then
    echo "FAIL: launch가 종료됨 — 로그 끝부분:"; tail -30 "$LOG"; exit 1
  fi
  LIST=$(ros2 topic list 2>/dev/null || true)
  MISSING=()
  for tp in "${TOPICS[@]}"; do grep -qx "$tp" <<<"$LIST" || MISSING+=("$tp"); done
  [ ${#MISSING[@]} -eq 0 ] && break
  sleep 2
done
[ ${#MISSING[@]} -gt 0 ] && echo "⚠ 시간 안에 안 뜬 토픽: ${MISSING[*]} (record_scene.sh가 진행 여부를 물음)"

# --- 녹화 (Ctrl+C는 녹화만 멈추고 이 스크립트는 정리까지 계속)
trap ':' INT
"$HERE/record_scene.sh" "$NAME" "${TOPICS[@]}" || true
trap - INT

ros2 bag info "$HERE/$NAME" 2>/dev/null | grep -E "Duration|Topic:" || true
