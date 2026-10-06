#!/usr/bin/env bash
# 사용법: ./record_scene.sh            ← 아래 설정값으로 녹화
#         ./record_scene.sh [이름] [토픽...]  ← 인자를 주면 설정값 대신 사용
#   - 로봇 bringup·카메라 노드는 다른 터미널에서 먼저 실행해 둘 것
#   - 녹화가 끝나면 README.md [목록]에 [목록:파일명] 항목 자동 등록 (같은 파일이면 교체)

# ===== 설정 (여기만 고치면 됨) =====
SCENE_NAME="scene3"                 # 비워 두면("") scene1, scene2 ... 중 빈 번호 자동
DEFAULT_TOPICS=(/image_raw /odom)
SCENE_DESC=""                       # README '장면' 칸, 비워 두면 녹화 후 입력받음
# ==================================

set -e
cd "$(dirname "$0")"

command -v ros2 >/dev/null || { echo "ros2 없음 — source /opt/ros/<배포판>/setup.bash 먼저"; exit 1; }

# 이름: 인자 > SCENE_NAME > 비어 있는 sceneN
if [ -n "$1" ]; then
  NAME="${1%/}"; shift
elif [ -n "$SCENE_NAME" ]; then
  NAME="$SCENE_NAME"
else
  n=1; while [ -e "scene$n" ] || [ -e "scene$n.tar.gz" ]; do n=$((n+1)); done
  NAME="scene$n"
fi
[ -e "$NAME" ] && { echo "$NAME 폴더가 이미 있음 — 스크립트 위쪽 SCENE_NAME을 바꾸세요"; exit 1; }

if [ $# -gt 0 ]; then TOPICS=("$@"); else TOPICS=("${DEFAULT_TOPICS[@]}"); fi

# 토픽 확인
echo "---- 현재 발행 중인 토픽 ----"
LIST=$(ros2 topic list 2>/dev/null || true)
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

# 녹화 → 압축 → SHA256SUMS
./pack_bag.sh "$NAME" "${TOPICS[@]}"

# ---- README.md [목록]에 등록 ----
FILE="${NAME}.tar.gz"
INFO=$(ros2 bag info "$NAME" 2>/dev/null || true)

# 기간: "Duration: 10.93166s" → 10.9s
DUR=$(awk '/Duration:/ {gsub("s","",$2); printf "%.1fs", $2}' <<<"$INFO")

# 토픽: ROS 기본 토픽 제외, 메시지 1개 이상만 "이름(개수)"
TOPIC_COL=$(grep -oE "Topic: [^ ]+ \| Type: [^ ]+ \| Count: [0-9]+" <<<"$INFO" \
  | awk '{print $2, $8}' \
  | grep -vE "^/(rosout|parameter_events|events/write_split) " \
  | awk '$2 > 0 {printf "%s%s(%s)", sep, $1, $2; sep=", "}')

if [ -z "$TOPIC_COL" ]; then
  echo "⚠ 실제 데이터 토픽이 없는 bag입니다."
  read -rp "그래도 README에 등록할까요? (y/N): " ans
  [[ "$ans" == y || "$ans" == yes ]] || { echo "README 등록 생략"; exit 0; }
  TOPIC_COL="(데이터 없음)"
fi

SIZE=$(du -h "$FILE" | cut -f1)
SHA=$(sha256sum "$FILE" | cut -d" " -f1)
COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "-")
DESC="$SCENE_DESC"
[ -n "$DESC" ] || read -rp "장면 설명 (README '장면' 칸, 엔터=생략): " DESC
DESC="${DESC:--}"

ENTRY="[목록:$FILE]
- 장면: $DESC
- 기간: $DUR
- 토픽: $TOPIC_COL
- 용량: $SIZE
- SHA256: \`$SHA\`
- 기준 커밋: \`$COMMIT\`
- 다운로드: (업로드 후 기입)"

# 같은 파일 항목은 교체, 없으면 파일 끝에 추가 ("등록된 bag 없음" 줄은 제거)
python3 - "$FILE" "$ENTRY" <<'PY'
import sys
f, entry = sys.argv[1], sys.argv[2]
lines = open("README.md", encoding="utf-8").read().rstrip("\n").split("\n")
lines = [l for l in lines if not l.startswith("- 등록된 bag 없음")]
head = f"[목록:{f}]"
if head in lines:
    a = lines.index(head)
    b = a + 1
    while b < len(lines) and lines[b].startswith("- "):
        b += 1
    lines[a:b] = entry.split("\n")
else:
    if "[목록]" not in lines:
        lines += ["", "[목록]"]
    lines += [""] + entry.split("\n")
open("README.md", "w", encoding="utf-8").write("\n".join(lines) + "\n")
PY
echo "---- README.md 등록 ----"
echo "$ENTRY"
