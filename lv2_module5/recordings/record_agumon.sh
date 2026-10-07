#!/usr/bin/env bash
# 사용법: ./record_agumon.sh [토픽...]
#   Raspberry Pi(agumon.local, ROS_DOMAIN_ID=9)에서 돌고 있는 장비 노드의 토픽을 이 PC에서 녹화만 한다
#   - 이 PC에서는 노드를 띄우지 않음 (구독만) → Pi 설정·파일·모터 동작에 영향 없음
#   - Wi-Fi가 멀티캐스트를 막아서 ROS_STATIC_PEERS로 Pi 주소를 직접 지정
#   - 원본 영상(image_raw)은 Wi-Fi로 받기엔 커서 기본에서 제외, 디버그 영상(jpeg)만
#   - Ctrl+C로 녹화 종료 → pack_bag.sh가 압축·SHA256SUMS 기록 (README 등록은 하지 않음)
#   - 이름: agumon_YYYYmmdd_HHMMSS
#   다른 Pi·도메인: PI_HOST=xxx.local DOMAIN_ID=7 ./record_agumon.sh
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
PI_HOST="${PI_HOST:-agumon.local}"
DOMAIN_ID="${DOMAIN_ID:-9}"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-lyrical}"
DEFAULT_TOPICS=(/target /motor_cmd /tracking_status /perception_node/debug_image/compressed)
# ================

set -e
cd "$(dirname "$0")"

export ROS_DOMAIN_ID="$DOMAIN_ID"
export ROS_STATIC_PEERS="$PI_HOST"
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"

if [ $# -gt 0 ]; then TOPICS=("$@"); else TOPICS=("${DEFAULT_TOPICS[@]}"); fi
echo "=== ROS_DOMAIN_ID=$ROS_DOMAIN_ID ROS_STATIC_PEERS=$ROS_STATIC_PEERS"
./pack_bag.sh "agumon_$(date +%Y%m%d_%H%M%S)" "${TOPICS[@]}"
