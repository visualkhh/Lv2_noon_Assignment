#!/usr/bin/env bash
# 사용법: ./record_scene.sh            ← 아래 설정값으로 녹화
#         ./record_scene.sh [이름] [토픽...]  ← 인자를 주면 설정값 대신 사용
#   - 이 PC에서 따로 띄워 둔 노드(bringup·카메라 등)를 녹화 (로컬 모드)
#     상대 기기의 토픽을 녹화하려면 record_peer.sh, 노드 실행까지 한 번에 하려면 run_and_record.sh
#   - 녹화가 끝나면 README.md [목록]에 [목록:파일명] 항목 자동 등록 (같은 파일이면 교체)
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 (여기만 고치면 됨) =====
SCENE_NAME=""                       # 비워 두면("") scene1, scene2 ... 중 빈 번호 자동
DEFAULT_TOPICS=(/target /motor_cmd /tracking_status /camera/camera/color/image_raw)
SCENE_DESC=""                       # README '장면' 칸, 비워 두면 녹화 후 입력받음
# ==================================

set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$HERE/env.sh" --local

# 이름: 인자 > SCENE_NAME > 비어 있는 sceneN
if [ -n "$1" ] && [[ "$1" != /* ]]; then
  NAME="${1%/}"; shift
elif [ -n "$SCENE_NAME" ]; then
  NAME="$SCENE_NAME"
else
  n=1; while [ -e "$BAG_DIR/scene$n" ] || [ -e "$BAG_DIR/scene$n.tar.gz" ] || [ -e "$BAG_DIR/_empty/scene$n" ]; do n=$((n+1)); done
  NAME="scene$n"
fi
[ -e "$BAG_DIR/$NAME" ] && { echo "bags/$NAME 폴더가 이미 있음 — 스크립트 위쪽 SCENE_NAME을 바꾸세요"; exit 1; }

if [ $# -gt 0 ]; then TOPICS=("$@"); else TOPICS=("${DEFAULT_TOPICS[@]}"); fi

# 토픽 확인
echo "---- 현재 발행 중인 토픽 ----"
LIST=$(ros2 topic list --no-daemon 2>/dev/null || true)
echo "$LIST"
MISSING=()
for t in "${TOPICS[@]}"; do
  grep -qx "$t" <<<"$LIST" || MISSING+=("$t")
done
if [ ${#MISSING[@]} -gt 0 ]; then
  echo "⚠ 아직 안 보이는 토픽: ${MISSING[*]}"
  echo "  (bringup·카메라 노드 실행 여부, 토픽 이름 확인)"
  read -rp "그래도 녹화할까요? (y/N): " ans
  [[ "$ans" == y || "$ans" == yes ]] || { echo "중단"; exit 1; }
fi

# 녹화 → 압축 → SHA256SUMS → README 등록
"$HERE/pack_bag.sh" "$NAME" "${TOPICS[@]}"
"$HERE/register_bag.sh" "$NAME" "$SCENE_DESC"
