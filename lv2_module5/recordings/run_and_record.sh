#!/usr/bin/env bash
# 사용법: ./run_and_record.sh [이름] [launch 인자...]
#   ./run_and_record.sh                          ← sim_YYYYmmdd_HHMMSS 이름으로, 실행 후 녹화
#   ./run_and_record.sh scene5                   ← 이름 지정
#   ./run_and_record.sh scene5 use_tracker:=false ← launch 인자는 sim.launch.py로 그대로 전달
#   - 현재 브랜치의 cognitive_control을 빌드하고 sim.launch.py를 백그라운드로 실행
#     (기기가 없으면 가상, RealSense·OpenCR를 꽂으면 자동으로 실기 전환)
#   - 토픽이 뜨면 녹화 시작, Ctrl+C로 녹화 종료
#     → register_bag.sh: 압축·SHA256SUMS·README [목록] 등록 (장면 설명 입력 + 카메라·OpenCR 상태 자동)
#   - sim_gui.sh와 달리 원본 영상(image_raw)을 녹화하고 GUI 웹 서버는 띄우지 않음
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
TOPICS=(/camera/camera/color/image_raw /target /motor_cmd /tracking_status)   # 기다렸다가 녹화
EXTRA_TOPICS=(/camera_source /opencr_status /joint_states /virtual_target)    # 상태·3D (같이 녹화)
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

# --- 노드 실행 (별도 프로세스 그룹: 녹화 중 Ctrl+C가 launch까지 죽이지 않도록)
LOG="${TMPDIR:-/tmp}/$NAME.launch.log"
echo "=== ros2 launch cognitive_control sim.launch.py $* (로그: $LOG)"
setsid ros2 launch cognitive_control sim.launch.py "$@" >"$LOG" 2>&1 &
SIM=$!

stop_sim() {
  kill -0 "$SIM" 2>/dev/null || return 0
  echo "=== 노드 종료"
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
if [ ${#MISSING[@]} -gt 0 ]; then
  echo "⚠ 시간 안에 안 뜬 토픽: ${MISSING[*]} (노드 실행·토픽 이름 확인, 로그: $LOG)"
  if [ -t 0 ]; then
    read -rp "그래도 녹화할까요? (y/N): " ans
    [[ "$ans" == y || "$ans" == yes ]] || { echo "중단"; exit 1; }
  fi
fi

# --- 녹화 (Ctrl+C는 녹화만 멈추고 이 스크립트는 등록까지 계속)
echo "녹화 시작: recordings/$NAME (${TOPICS[*]} ${EXTRA_TOPICS[*]}) — 끝내려면 Ctrl+C"
trap ':' INT
(cd "$HERE" && ros2 bag record -o "$NAME" "${TOPICS[@]}" "${EXTRA_TOPICS[@]}") || true
trap - INT

# --- 압축·SHA256SUMS·README 등록
stop_sim
"$HERE/register_bag.sh" "$NAME"
