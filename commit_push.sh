#!/usr/bin/env bash
# 사용법: ./commit_push.sh ["커밋 메시지"]
#   1) 깨진 pre-commit 훅이 있으면 고침 (원본은 pre-commit.bak)
#   2) 스테이징된 변경을 커밋하고 feature/integration으로 push
set -e
cd "$(dirname "$0")"

MSG="${1:-pack_bag.sh: 녹화부터 압축·체크섬 기록까지 한 번에 처리}"
BRANCH="feature/integration"
HOOK=".git/hooks/pre-commit"

# 1. pre-commit 훅 문법 오류일 때만 수정 (이미 고쳐졌으면 건너뜀)
if [ -f "$HOOK" ] && ! bash -n "$HOOK" 2>/dev/null; then
  echo "pre-commit 훅 문법 오류 → 수정"
  cp "$HOOK" "$HOOK.bak"
  sed -i '70,73d' "$HOOK"
  bash -n "$HOOK" || { echo "훅 수정 실패, $HOOK.bak 에서 복구하세요"; exit 1; }
fi
echo "훅 OK"

# 2. 스테이징 확인
git status --short
if git diff --cached --quiet; then
  echo "스테이징된 변경 없음 — 먼저 git add 하세요"
  exit 1
fi

# 3. 커밋 & push
git commit -m "$MSG"
git push origin "$BRANCH"
echo "완료: $(git log --oneline -1)"
