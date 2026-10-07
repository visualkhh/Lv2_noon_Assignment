#!/usr/bin/env bash
# 사용법: ./record_agumon.sh [토픽...]
#   라즈베리파이(agumon.local, ROS_DOMAIN_ID=9) 전용 바로가기 → record_peer.sh
#   이름: agumon_YYYYmmdd_HHMMSS
#   다른 Pi·도메인: PEERS=xxx.local DOMAIN_ID=7 ./record_agumon.sh  (또는 ./connect.sh xxx.local --domain 7 로 저장)
HERE="$(cd "$(dirname "$0")" && pwd)"
export PEERS="${PEERS:-${PI_HOST:-agumon.local}}" DOMAIN_ID="${DOMAIN_ID:-9}" NAME_PREFIX=agumon
exec "$HERE/record_peer.sh" "$@"
