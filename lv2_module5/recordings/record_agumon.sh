#!/usr/bin/env bash
# 사용법: ./record_agumon.sh [토픽...]
#   라즈베리파이(agumon.local) 전용 바로가기 → record_peer.sh (ROS_DOMAIN_ID는 팀 기본값 63, env.sh의 LV2_TEAM_DOMAIN)
#   이름: agumon_YYYYmmdd_HHMMSS
#   다른 Pi·도메인: PEERS=xxx.local DOMAIN_ID=7 ./record_agumon.sh  (또는 ./connect.sh xxx.local --domain 7 로 저장)
HERE="$(cd "$(dirname "$0")" && pwd)"
export PEERS="${PEERS:-${PI_HOST:-agumon.local}}" NAME_PREFIX=agumon
exec "$HERE/record_peer.sh" "$@"
