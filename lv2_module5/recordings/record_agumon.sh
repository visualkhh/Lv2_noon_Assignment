#!/usr/bin/env bash
# 사용법: ./record_agumon.sh [토픽...]
#   라즈베리파이(agumon.local) 전용 바로가기 → record_peer.sh (ROS_DOMAIN_ID는 라즈베리파이 쪽 값을 자동으로 찾음)
#   이름: agumon_YYYYmmdd_HHMMSS
#   다른 Pi: PEERS=xxx.local ./record_agumon.sh   · 도메인 직접 지정: DOMAIN_ID=7 ./record_agumon.sh
HERE="$(cd "$(dirname "$0")" && pwd)"
export PEERS="${PEERS:-${PI_HOST:-agumon.local}}" NAME_PREFIX=agumon
exec "$HERE/record_peer.sh" "$@"
