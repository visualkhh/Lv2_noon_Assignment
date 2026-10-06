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

if [ -d "$NAME" ]; then
  [ $# -gt 0 ] && echo "※ $NAME 폴더가 이미 있어 녹화는 건너뜀 (새로 녹화하려면 다른 이름 사용)"
else
  if [ $# -gt 0 ]; then TOPICS=("$@"); else TOPICS=(-a); fi
  echo "---- 현재 발행 중인 토픽 ----"
  ros2 topic list || true
  echo "녹화 시작: $NAME (${TOPICS[*]}) — 끝내려면 Ctrl+C"
  trap ':' INT                       # Ctrl+C는 녹화만 멈추고 스크립트는 계속
  ros2 bag record -o "$NAME" "${TOPICS[@]}" || true
  trap - INT
fi

[ -f "$NAME/metadata.yaml" ] || { echo "bag 폴더 없음 또는 손상: $NAME"; exit 1; }

# /rosout 등 ROS 기본 토픽 외에 녹화된 메시지가 없으면 경고
DATA=$(ros2 bag info "$NAME" 2>/dev/null | grep "Topic:" \
  | grep -vE "Topic: /(rosout|parameter_events|events/write_split) " \
  | grep -vE "Count: 0 " || true)
[ -n "$DATA" ] || echo "⚠ 경고: 실제 데이터 토픽이 녹화되지 않음 (로봇·카메라 노드 실행 여부 확인)"

tar -czf "${NAME}.tar.gz" "$NAME"
# 같은 파일의 기존 체크섬 줄은 지우고 새로 기록 (중복 방지)
touch SHA256SUMS
grep -vF "  ${NAME}.tar.gz" SHA256SUMS > SHA256SUMS.tmp || true
mv SHA256SUMS.tmp SHA256SUMS
sha256sum "${NAME}.tar.gz" | tee -a SHA256SUMS

echo "---- 표에 옮길 정보 ----"
echo "용량: $(du -h "${NAME}.tar.gz" | cut -f1)"
echo "기준 커밋: $(git rev-parse --short HEAD)"
ros2 bag info "$NAME" | grep -E "Duration|Topic:" || true

echo "---- SHA256SUMS ----"
cat SHA256SUMS
