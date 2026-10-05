#!/usr/bin/env bash
# 사용법: ./pack_bag.sh <bag 폴더명>   (recordings 폴더 안에서 실행)
set -e
NAME="${1%/}"
[ -d "$NAME" ] || { echo "bag 폴더 없음: $NAME"; exit 1; }

tar -czf "${NAME}.tar.gz" "$NAME"
sha256sum "${NAME}.tar.gz" | tee -a SHA256SUMS

echo "---- 표에 옮길 정보 ----"
echo "용량: $(du -h "${NAME}.tar.gz" | cut -f1)"
echo "기준 커밋: $(git rev-parse --short HEAD)"
ros2 bag info "$NAME" | grep -E "Duration|Topic:" || true
