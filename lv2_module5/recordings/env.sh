# shellcheck shell=bash
# recordings 공통 환경 — 다른 스크립트가 source 한다 (직접 실행하지 않음)
#   source env.sh          ← 로컬 모드: 이 PC 안에서만 통신 (다른 기기·로봇과 섞이지 않음)
#   source env.sh --link   ← 연동 모드: connect.sh로 저장한 상대 기기(PEERS·DOMAIN_ID)와 통신
#
# 정하는 값
#   REC_DIR  이 폴더 (어디로 옮기거나 zip으로 받아도 이 파일 위치 기준)
#   BAG_DIR  REC_DIR/bags — bag 폴더와 .tar.gz
#   ROS      ROS_SETUP > ROS_DISTRO_NAME > connect.sh 저장값 > 이미 source된 배포판 > /opt/ros 자동 탐지
#   WS       LV2_WS > ../ros2_ws > 저장값 (cognitive_control이 있는 워크스페이스, 없으면 빈 값 = 녹화·재생 전용)
#   네트워크 RMW는 rmw_fastrtps_cpp로 통일 (jazzy·lyrical 모두 기본 설치, 서로 통신 확인됨)
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

REC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BAG_DIR="$REC_DIR/bags"
LINK_FILE="$REC_DIR/.link.env"     # connect.sh가 기록하는 기기별 설정 (git·zip에 포함하지 않음)
# 팀 공통 ROS_DOMAIN_ID — 2026-10-07 강의실 Wi-Fi 스캔에서 0·28·30·42·50·87이 다른 기기에 쓰이고 있어 비어 있는 63으로 정함
#   바꿀 때는 여기만 고치면 connect.sh·record_agumon.sh 기본값이 따라감 (모든 팀원 기기가 같은 값이어야 함)
LV2_TEAM_DOMAIN=63
mkdir -p "$BAG_DIR"

# shellcheck disable=SC1090
[ -f "$LINK_FILE" ] && . "$LINK_FILE"

# 이미 source된 다른 ROS 경로를 PATH 등에서 걷어낸다 (jazzy·lyrical이 섞이면 import가 꼬임)
lv2_scrub_ros() {
  local prefixes="${AMENT_PREFIX_PATH}:${COLCON_PREFIX_PATH}" v p q out keep
  local -a parts pre
  IFS=: read -ra pre <<<"$prefixes"
  for v in PATH LD_LIBRARY_PATH PYTHONPATH AMENT_PREFIX_PATH CMAKE_PREFIX_PATH COLCON_PREFIX_PATH PKG_CONFIG_PATH; do
    out=""
    IFS=: read -ra parts <<<"${!v}"
    for p in "${parts[@]}"; do
      [ -n "$p" ] || continue
      keep=1
      case "$p" in /opt/ros/*) keep=0 ;; esac
      for q in "${pre[@]}"; do [ -n "$q" ] && [[ "$p" == "$q" || "$p" == "$q"/* ]] && keep=0; done
      [ "$keep" = 1 ] && out="${out:+$out:}$p"
    done
    export "$v=$out"
  done
  unset ROS_DISTRO ROS_VERSION ROS_PYTHON_VERSION ROS_ETC_DIR ROS_PACKAGE_PATH
}

# 사용할 ROS setup.bash 경로를 출력
lv2_ros_setup() {
  local d f
  if [ -n "$ROS_SETUP" ]; then [ -f "$ROS_SETUP" ] && echo "$ROS_SETUP"; return; fi
  for d in "$ROS_DISTRO_NAME" "$LINK_DISTRO" "$ROS_DISTRO"; do
    [ -n "$d" ] && [ -f "/opt/ros/$d/setup.bash" ] && { echo "/opt/ros/$d/setup.bash"; return; }
  done
  [ -n "$LINK_SETUP" ] && [ -f "$LINK_SETUP" ] && { echo "$LINK_SETUP"; return; }
  for d in lyrical kilted jazzy humble; do
    [ -f "/opt/ros/$d/setup.bash" ] && { echo "/opt/ros/$d/setup.bash"; return; }
  done
  for f in /opt/ros/*/setup.bash; do [ -f "$f" ] && { echo "$f"; return; }; done
  return 1
}

# cognitive_control 워크스페이스 경로를 출력 (없으면 실패)
lv2_find_ws() {
  local c
  for c in "$LV2_WS" "$REC_DIR/../ros2_ws" "$LINK_WS"; do
    [ -n "$c" ] && [ -f "$c/src/cognitive_control/package.xml" ] && { (cd "$c" && pwd); return; }
  done
  return 1
}

# 워크스페이스 빌드 — 다른 배포판으로 빌드된 흔적이 있으면 build·install·log를 지우고 새로 빌드
lv2_build() {
  [ -n "$WS" ] || { echo "워크스페이스 없음 — 빌드 생략 (녹화·재생 전용)"; return 1; }
  if [ -f "$WS/install/setup.sh" ] && ! grep -qF "$ROS_PREFIX" "$WS/install/setup.sh"; then
    echo "=== 다른 ROS 배포판으로 빌드된 워크스페이스 → 정리 후 $ROS_DISTRO로 다시 빌드"
    rm -rf -- "$WS/build" "$WS/install" "$WS/log"
  fi
  (cd "$WS" && colcon build --symlink-install --packages-select cognitive_control) || return 1
  # shellcheck disable=SC1091
  source "$WS/install/setup.bash"
}

# ---- ROS source
ROS_SETUP_FILE="$(lv2_ros_setup)" || ROS_SETUP_FILE=""
if [ -z "$ROS_SETUP_FILE" ]; then
  echo "ROS 2를 찾지 못함 — /opt/ros/<배포판> 설치, 또는 ROS_SETUP=<setup.bash 경로> 지정 (Docker는 connect.sh --docker)" >&2
  return 1 2>/dev/null || exit 1
fi
ROS_PREFIX="$(cd "$(dirname "$ROS_SETUP_FILE")" && pwd)"
if [ -n "$AMENT_PREFIX_PATH" ] && [[ ":$AMENT_PREFIX_PATH:" != *":$ROS_PREFIX:"* ]]; then
  lv2_scrub_ros
fi
# shellcheck disable=SC1090
source "$ROS_SETUP_FILE"
WS="$(lv2_find_ws)" || WS=""

# ---- 네트워크
export RMW_IMPLEMENTATION="${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}"
if [ "$1" = "--link" ] || [ "${LINK:-0}" = 1 ]; then
  LV2_MODE=link
  export ROS_DOMAIN_ID="${DOMAIN_ID:-${LINK_DOMAIN_ID:-$LV2_TEAM_DOMAIN}}"
  export ROS_AUTOMATIC_DISCOVERY_RANGE=SUBNET
  PEERS="${PEERS:-$LINK_PEERS}"
  if [ -n "$PEERS" ]; then export ROS_STATIC_PEERS="$PEERS"; else unset ROS_STATIC_PEERS; fi
  unset ROS_LOCALHOST_ONLY
else
  LV2_MODE=local
  export ROS_DOMAIN_ID="${DOMAIN_ID:-${ROS_DOMAIN_ID:-0}}"
  export ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST
  unset ROS_STATIC_PEERS
  if [ "$ROS_DISTRO" = humble ]; then export ROS_LOCALHOST_ONLY=1; fi   # humble은 DISCOVERY_RANGE 미지원
fi
true
