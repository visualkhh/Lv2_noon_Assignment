#!/usr/bin/env bash
# 사용법: ./register_bag.sh <bag 이름> [장면 설명]
#   - 이미 녹화된 bag 폴더를 pack_bag.sh로 압축·SHA256SUMS 기록 후 README.md [목록]에 등록
#   - 같은 파일 항목이 있으면 교체
#   - 설명을 안 주면 터미널에서 입력받음 (터미널이 아니면 "-")
#   - 데이터 토픽이 없는 bag: 터미널이면 등록 여부를 묻고, 아니면 등록하지 않음
set -e
cd "$(dirname "$0")"

NAME="${1%/}"
DESC="$2"
[ -n "$NAME" ] || { sed -n '2,6p' "$0"; exit 1; }
[ -f "$NAME/metadata.yaml" ] || { echo "bag 폴더 없음 또는 손상: $NAME"; exit 1; }
command -v ros2 >/dev/null || { echo "ros2 없음 — source /opt/ros/<배포판>/setup.bash 먼저"; exit 1; }

./pack_bag.sh "$NAME"     # 폴더가 있으므로 녹화 없이 압축·체크섬만

FILE="${NAME}.tar.gz"
INFO=$(ros2 bag info "$NAME" 2>/dev/null || true)

# 기간: "Duration: 10.93166s" → 10.9s
DUR=$(awk '/Duration:/ {gsub("s","",$2); printf "%.1fs", $2}' <<<"$INFO")

# 토픽: ROS 기본 토픽 제외, 메시지 1개 이상만 "이름(개수)"
TOPIC_COL=$(grep -oE "Topic: [^ ]+ \| Type: [^ ]+ \| Count: [0-9]+" <<<"$INFO" \
  | awk '{print $2, $8}' \
  | grep -vE "^/(rosout|parameter_events|events/write_split) " \
  | awk '$2 > 0 {printf "%s%s(%s)", sep, $1, $2; sep=", "}' || true)

if [ -z "$TOPIC_COL" ]; then
  echo "⚠ 실제 데이터 토픽이 없는 bag입니다."
  if [ -t 0 ]; then
    read -rp "그래도 README에 등록할까요? (y/N): " ans
    [[ "$ans" == y || "$ans" == yes ]] || { echo "README 등록 생략"; exit 0; }
  else
    echo "README 등록 생략"; exit 0
  fi
  TOPIC_COL="(데이터 없음)"
fi

if [ -z "$DESC" ] && [ -t 0 ]; then
  read -rp "장면 설명 (README '장면' 칸, 엔터=생략): " DESC
fi
DESC="${DESC:--}"

SIZE=$(du -h "$FILE" | cut -f1)
SHA=$(sha256sum "$FILE" | cut -d" " -f1)
COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "-")

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
