#!/usr/bin/env bash
# 사용법: ./run_and_record.sh [이름] [launch 인자...]
#   ./run_and_record.sh                          ← 빈 번호 sceneN으로, 카메라 + 모터 실행 후 녹화
#   ./run_and_record.sh scene5                   ← 이름 지정
#   ./run_and_record.sh scene5 use_motor:=false  ← launch 인자는 start.sh로 그대로 전달
#   - feature/control_perception 코드를 ../lv2_perception worktree로 받아 start.sh로 bringup (백그라운드)
#   - 토픽이 뜨면 record_scene.sh로 녹화, Ctrl+C로 녹화 종료 → 압축·README 등록 → bringup 종료
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
BRANCH=origin/feature/control_perception
TOPICS=(/camera/camera/color/image_raw /target /motor_cmd /tracking_status)
WAIT_SEC=600                        # 빌드 + 노드 기동 대기 최대 시간
# ================

set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
WT="$(dirname "$REPO")/lv2_perception"
WS="$WT/lv2_module5/ros2_ws"

# 이름: ':='가 없는 첫 인자 > 비어 있는 sceneN
if [ -n "$1" ] && [[ "$1" != *:=* ]]; then
  NAME="${1%/}"; shift
else
  n=1; while [ -e "$HERE/scene$n" ] || [ -e "$HERE/scene$n.tar.gz" ]; do n=$((n+1)); done
  NAME="scene$n"
fi
[ -e "$HERE/$NAME" ] && { echo "$NAME 폴더가 이미 있음 — 다른 이름을 주세요"; exit 1; }

# --- ROS 환경
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"

# --- 실행 코드 (worktree, 없으면 생성)
if [ ! -x "$WS/start.sh" ]; then
  echo "=== $BRANCH → $WT"
  git -C "$REPO" fetch origin
  git -C "$REPO" worktree add "$WT" "$BRANCH"
fi

# --- bringup (별도 프로세스 그룹: 녹화 중 Ctrl+C가 bringup까지 죽이지 않도록)
LOG="${TMPDIR:-/tmp}/bringup_$NAME.log"
echo "=== bringup: $WS/start.sh --build $* (로그: $LOG)"
ROS_DISTRO_NAME="$ROS_DISTRO_NAME" setsid "$WS/start.sh" --build "$@" >"$LOG" 2>&1 &
BRINGUP=$!

stop_bringup() {
  kill -0 "$BRINGUP" 2>/dev/null || return 0
  echo "=== bringup 종료"
  kill -INT -- -"$BRINGUP" 2>/dev/null || true
  for _ in $(seq 20); do kill -0 "$BRINGUP" 2>/dev/null || return 0; sleep 0.5; done
  kill -TERM -- -"$BRINGUP" 2>/dev/null || true
}
trap stop_bringup EXIT

# --- 토픽 대기
echo "=== 토픽 대기 (최대 ${WAIT_SEC}s): ${TOPICS[*]}"
for ((t = 0; t < WAIT_SEC; t += 2)); do
  if ! kill -0 "$BRINGUP" 2>/dev/null; then
    echo "FAIL: bringup이 종료됨 — 로그 끝부분:"; tail -30 "$LOG"; exit 1
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
