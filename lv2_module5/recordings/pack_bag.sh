#!/usr/bin/env bash
# 사용법: ./pack_bag.sh [bag 이름] [토픽...]
#   - bag 폴더가 없으면 먼저 녹화 (토픽 생략 시 전체 -a), Ctrl+C로 녹화 종료
#   - 이미 있으면 녹화 없이 바로 압축
#   - 이름 생략 시 scene_YYYYmmdd_HHMMSS
set -e
cd "$(dirname "$0")"   # 어디서 실행해도 recordings 폴더 기준

NAME="${1%/}"
NAME="${NAME:-scene_$(date +%Y%m%d_%H%M%S)}"
shift || true

if [ ! -d "$NAME" ]; then
  if [ $# -gt 0 ]; then TOPICS=("$@"); else TOPICS=(-a); fi
  echo "녹화 시작: $NAME (${TOPICS[*]}) — 끝내려면 Ctrl+C"
  trap ':' INT                       # Ctrl+C는 녹화만 멈추고 스크립트는 계속
  ros2 bag record -o "$NAME" "${TOPICS[@]}" || true
  trap - INT
fi

[ -f "$NAME/metadata.yaml" ] || { echo "bag 폴더 없음 또는 손상: $NAME"; exit 1; }

tar -czf "${NAME}.tar.gz" "$NAME"
sha256sum "${NAME}.tar.gz" | tee -a SHA256SUMS

echo "---- 표에 옮길 정보 ----"
echo "용량: $(du -h "${NAME}.tar.gz" | cut -f1)"
echo "기준 커밋: $(git rev-parse --short HEAD)"
ros2 bag info "$NAME" | grep -E "Duration|Topic:" || true

echo "---- SHA256SUMS ----"
cat SHA256SUMS
