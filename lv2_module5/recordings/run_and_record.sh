#!/usr/bin/env bash
# 사용법: ./run_and_record.sh [이름] [launch 인자...]
#   ./run_and_record.sh                          ← sim_YYYYmmdd_HHMMSS 이름으로 실행·녹화
#   ./run_and_record.sh scene5                   ← 이름 지정
#   ./run_and_record.sh scene5 use_tracker:=false ← launch 인자는 sim.launch.py로 그대로 전달
#   NO_RECORD=1 ./run_and_record.sh              ← 실행만 (녹화·등록 안 함)
#   RECORD_RAW=0 ./run_and_record.sh             ← 원본 영상 대신 디버그 영상(jpeg)으로 가볍게 녹화
#   SCENE_DESC="..." ./run_and_record.sh         ← README '장면' 칸 (없으면 끝날 때 입력받음)
#   LINK=1 ./run_and_record.sh                   ← connect.sh로 연동한 상대 기기와 같은 네트워크에서 실행
#                                                  (기본은 로컬 모드: 이 PC 밖으로 /motor_cmd가 나가지 않음)
#
# 순서: ① ROS 자동 탐지·빌드 → ② sim.launch.py 실행 (기기가 없으면 가상, RealSense·OpenCR를 꽂으면 자동 실기 전환)
#       → ③ 토픽 대기 → ④ 녹화 (Ctrl+C로 종료) → ⑤ 노드 종료
#       → ⑥ pack_bag.sh로 압축·SHA256SUMS → ⑦ register_bag.sh로 README.md [목록] 등록
# sim_gui.sh는 GUI 웹 서버를 띄운 뒤 이 스크립트를 그대로 실행한다.
# 워크스페이스(../ros2_ws 또는 LV2_WS)가 필요하다 — recordings만 받은 경우는 record_peer.sh / play_bag.sh 사용.
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
WAIT_TOPICS=(/target /motor_cmd /tracking_status)                 # 뜰 때까지 기다리는 토픽
REC_TOPICS=(/target /motor_cmd /tracking_status /camera_source /opencr_status
            /joint_states /virtual_target)                        # 상태·제어·3D
if [ "${RECORD_RAW:-1}" = 1 ]; then
  REC_TOPICS+=(/camera/camera/color/image_raw)                    # 원본 영상 (약 13MB/s)
else
  REC_TOPICS+=(/perception_node/debug_image/compressed)           # 디버그 영상 (jpeg)
fi
WAIT_SEC=60                         # 노드 기동 대기 최대 시간
# ================

set -eo pipefail
# Ctrl+C는 오류가 아니라 정상 종료로 처리한다
#   녹화 전(빌드·토픽 대기): 녹화 없이 종료 (exit 0)
#   녹화 중: 녹화만 멈추고 압축·등록까지 진행
#   녹화 후(압축·등록): 무시 — 중간에 끊겨 bag·README가 반쯤 써지는 것을 막음
trap 'echo; echo "=== 녹화 전에 중단 (Ctrl+C) — 녹화·등록 없이 종료"; exit 0' INT
HERE="$(cd "$(dirname "$0")" && pwd)"

# 이름: ':='가 없는 첫 인자 > sim_YYYYmmdd_HHMMSS
if [ -n "$1" ] && [[ "$1" != *:=* ]]; then
  NAME="${1%/}"; shift
else
  NAME="sim_$(date +%Y%m%d_%H%M%S)"
fi
[ -e "$HERE/bags/$NAME" ] && { echo "bags/$NAME 폴더가 이미 있음 — 다른 이름을 주세요"; exit 1; }

# --- ① ROS 환경 + 빌드
MODE_ARG=--local; [ "${LINK:-0}" = 1 ] && MODE_ARG=--link
# shellcheck disable=SC1091
source "$HERE/env.sh" "$MODE_ARG"
echo "=== ROS $ROS_DISTRO ($ROS_PREFIX) · $LV2_MODE 모드 · ROS_DOMAIN_ID=$ROS_DOMAIN_ID${ROS_STATIC_PEERS:+ · PEERS=$ROS_STATIC_PEERS}"
[ -n "$WS" ] || { echo "FAIL: cognitive_control 워크스페이스 없음 — ./connect.sh로 경로 지정 (LV2_WS=...)"; exit 1; }
lv2_build

# --- ② 노드 실행 (별도 프로세스 그룹: 녹화 중 Ctrl+C가 launch까지 죽이지 않도록)
if ss -ltn 2>/dev/null | grep -q ':9090 '; then
  echo "⚠ 9090 포트가 이미 사용 중 — 이전 실행의 rosbridge가 남아 있으면 GUI가 그쪽에 붙음"
  echo "  정리: pkill -f rosbridge_websocket"
fi
LOG="${TMPDIR:-/tmp}/$NAME.launch.log"
echo "=== ros2 launch cognitive_control sim.launch.py $* (로그: $LOG)"
setsid ros2 launch cognitive_control sim.launch.py "$@" >"$LOG" 2>&1 &
SIM=$!

stop_sim() {
  # launch가 먼저 끝나도 rosbridge 등 자식이 남을 수 있어 프로세스 그룹 전체가 사라질 때까지 확인한다
  # (남으면 9090 포트를 잡고 있어 다음 실행의 GUI 연결이 꼬인다)
  group_alive() { pgrep -g "$SIM" >/dev/null 2>&1; }
  group_alive || return 0
  echo "=== 노드 종료"
  local sig
  for sig in INT TERM KILL; do
    kill -"$sig" -- -"$SIM" 2>/dev/null || true
    for _ in $(seq 10); do group_alive || return 0; sleep 0.5; done
  done
}
trap stop_sim EXIT

if [ "${NO_RECORD:-0}" = 1 ]; then
  echo "=== 녹화 없이 실행 중 — 끝내려면 Ctrl+C"
  trap ':' INT
  wait "$SIM" || true
  exit 0
fi

# --- ③ 토픽 대기 (--no-daemon: 다른 배포판이 띄운 ros2 daemon과 섞이지 않도록)
echo "=== 토픽 대기 (최대 ${WAIT_SEC}s): ${WAIT_TOPICS[*]}"
for ((t = 0; t < WAIT_SEC; t += 2)); do
  if ! kill -0 "$SIM" 2>/dev/null; then
    echo "FAIL: launch가 종료됨 — 로그 끝부분:"; tail -30 "$LOG"; exit 1
  fi
  LIST=$(ros2 topic list --no-daemon 2>/dev/null || true)
  MISSING=()
  for tp in "${WAIT_TOPICS[@]}"; do grep -qx "$tp" <<<"$LIST" || MISSING+=("$tp"); done
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

# --- ④ 녹화 (Ctrl+C는 녹화만 멈추고 이 스크립트는 등록까지 계속)
echo "=== 녹화 시작: bags/$NAME (${REC_TOPICS[*]}) — 끝내려면 Ctrl+C"
trap ':' INT
ros2 bag record -s mcap -o "$BAG_DIR/$NAME" --topics "${REC_TOPICS[@]}" || true

trap '' INT
echo "=== 녹화 종료 — 압축·등록 중 (이제 Ctrl+C는 무시됨)"

# --- ⑤ 노드 종료
stop_sim

# --- ⑥ 압축·SHA256SUMS (폴더가 있으므로 pack_bag.sh는 녹화 없이 압축·체크섬만)
"$HERE/pack_bag.sh" "$NAME"

# --- ⑦ README.md [목록] 등록
"$HERE/register_bag.sh" "$NAME" "${SCENE_DESC:-}"
