#!/usr/bin/env bash
# 사용법: ./record_peer.sh [이름] [토픽...]
#   connect.sh로 연동한 상대 기기(라즈베리파이·조원 PC·Docker)에서 돌고 있는 노드의 토픽을 이 PC에서 녹화만 한다
#   - 이 PC에서는 노드를 띄우지 않음 (구독만) → 상대 설정·파일·모터 동작에 영향 없음
#   - 상대 주소·도메인은 .link.env(connect.sh가 저장) 사용, 일회성으로 바꾸려면 PEERS=... DOMAIN_ID=...
#   - 원본 영상(image_raw)은 Wi-Fi로 받기엔 커서 기본에서 제외, 디버그 영상(jpeg)만 (RECORD_RAW=1이면 포함)
#   - Ctrl+C로 녹화 종료 → 압축·SHA256SUMS → README [목록] 등록
#   - 이름 기본: peer_YYYYmmdd_HHMMSS (NAME_PREFIX로 앞부분 변경)
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
DEFAULT_TOPICS=(/target /motor_cmd /tracking_status /perception_node/debug_image/compressed
                /camera_source /opencr_status /joint_states)
[ "${RECORD_RAW:-0}" = 1 ] && DEFAULT_TOPICS+=(/camera/camera/color/image_raw)
# ================

set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$HERE/env.sh" --link

if [ -n "$1" ] && [[ "$1" != /* ]]; then NAME="${1%/}"; shift; else NAME="${NAME_PREFIX:-peer}_$(date +%Y%m%d_%H%M%S)"; fi
if [ $# -gt 0 ]; then TOPICS=("$@"); else TOPICS=("${DEFAULT_TOPICS[@]}"); fi
[ -e "$BAG_DIR/$NAME" ] && { echo "bags/$NAME 폴더가 이미 있음 — 다른 이름을 주세요"; exit 1; }
[ -n "$ROS_STATIC_PEERS" ] || echo "⚠ 상대 주소(PEERS) 미설정 — 같은 서브넷 멀티캐스트로만 찾음 (Wi-Fi면 ./connect.sh <상대 주소> 먼저)"

echo "=== ROS $ROS_DISTRO · ROS_DOMAIN_ID=$ROS_DOMAIN_ID · PEERS=${ROS_STATIC_PEERS:-(없음)}"
echo "=== 상대 토픽 찾는 중 (5s)..."
LIST=$(ros2 topic list --no-daemon --spin-time 5 2>/dev/null || true)
MISSING=()
for t in "${TOPICS[@]}"; do grep -qx "$t" <<<"$LIST" || MISSING+=("$t"); done
if [ ${#MISSING[@]} -eq ${#TOPICS[@]} ]; then
  echo "⚠ 녹화할 토픽이 하나도 안 보임 — 상대 노드 실행·주소·ROS_DOMAIN_ID 확인 (./connect.sh 로 점검)"
  if [ -t 0 ]; then read -rp "그래도 녹화할까요? (y/N): " ans; [[ "$ans" == y || "$ans" == yes ]] || exit 1; fi
elif [ ${#MISSING[@]} -gt 0 ]; then
  echo "※ 안 보이는 토픽(빠진 채로 녹화, 나중에 뜨면 같이 녹화됨): ${MISSING[*]}"
fi

"$HERE/pack_bag.sh" "$NAME" "${TOPICS[@]}"
"$HERE/register_bag.sh" "$NAME" "${SCENE_DESC:-}" || true
