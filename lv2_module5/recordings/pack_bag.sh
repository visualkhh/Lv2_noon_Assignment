#!/usr/bin/env bash
# 사용법: ./pack_bag.sh [bag 이름] [토픽...]
#   - bags/<이름> 폴더가 없으면 먼저 녹화 (토픽 생략 시 전체 -a), Ctrl+C로 녹화 종료
#   - 이미 있으면 녹화 없이 바로 압축
#   - 이름 생략 시 scene_YYYYmmdd_HHMMSS
#   - 결과: bags/<이름>.tar.gz + SHA256SUMS 한 줄 (같은 파일의 기존 줄은 교체)
#   - 녹화가 필요하면 ROS 환경이 없을 때 env.sh(로컬 모드)를 불러옴 — 연동 녹화는 record_peer.sh
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail
REC_DIR="$(cd "$(dirname "$0")" && pwd)"
BAG_DIR="$REC_DIR/bags"
mkdir -p "$BAG_DIR"

NAME="${1%/}"; NAME="${NAME#bags/}"
NAME="${NAME:-scene_$(date +%Y%m%d_%H%M%S)}"
shift || true

if [ -d "$BAG_DIR/$NAME" ]; then
  [ $# -gt 0 ] && echo "※ bags/$NAME 폴더가 이미 있어 녹화는 건너뜀 (새로 녹화하려면 다른 이름 사용)"
else
  # shellcheck disable=SC1091
  command -v ros2 >/dev/null || source "$REC_DIR/env.sh" --local
  if [ $# -gt 0 ]; then TOPICS=(--topics "$@"); else TOPICS=(-a); fi
  echo "---- 현재 발행 중인 토픽 ----"
  ros2 topic list --no-daemon || true
  echo "녹화 시작: bags/$NAME (${TOPICS[*]}) — 끝내려면 Ctrl+C"
  trap ':' INT                       # Ctrl+C는 녹화만 멈추고 스크립트는 계속
  ros2 bag record -s mcap -o "$BAG_DIR/$NAME" "${TOPICS[@]}" || true
  trap - INT
fi

[ -f "$BAG_DIR/$NAME/metadata.yaml" ] || { echo "bag 폴더 없음 또는 손상: bags/$NAME"; exit 1; }

# /rosout 등 ROS 기본 토픽 외에 녹화된 메시지가 없으면 경고
python3 - "$BAG_DIR/$NAME/metadata.yaml" <<'PY' || echo "⚠ 경고: 실제 데이터 토픽이 녹화되지 않음 (로봇·카메라 노드 실행 여부 확인)"
import sys, yaml
i = yaml.safe_load(open(sys.argv[1]))['rosbag2_bagfile_information']
skip = {'/rosout', '/parameter_events', '/events/write_split'}
sys.exit(0 if any(t['message_count'] > 0 and t['topic_metadata']['name'] not in skip
                  for t in i.get('topics_with_message_count', [])) else 1)
PY

tar -C "$BAG_DIR" -czf "$BAG_DIR/$NAME.tar.gz" "$NAME"
# SHA256SUMS는 recordings 기준 상대 경로(bags/...) — `sha256sum -c SHA256SUMS`로 검사
cd "$REC_DIR"
touch SHA256SUMS
grep -vE "  (bags/)?${NAME}\.tar\.gz$" SHA256SUMS > SHA256SUMS.tmp || true
mv SHA256SUMS.tmp SHA256SUMS
sha256sum "bags/$NAME.tar.gz" | tee -a SHA256SUMS

echo "---- 정보 ----"
echo "용량: $(du -h "bags/$NAME.tar.gz" | cut -f1)"
echo "기준 커밋: $(git rev-parse --short HEAD 2>/dev/null || echo -)"
