#!/usr/bin/env bash
# 사용법: ./play_bag.sh <이름> [ros2 bag play 옵션...]
#   ./play_bag.sh scene8                 ← bags/scene8 재생 (로컬 모드: 이 PC 안에서만)
#   ./play_bag.sh scene8 --loop -r 0.5   ← 옵션은 ros2 bag play로 그대로 전달
#   LINK=1 ./play_bag.sh scene8          ← 연동한 상대 기기 쪽으로 재생 (/motor_cmd는 기본 제외 — 로봇이 움직이지 않게)
#   LINK=1 WITH_MOTOR=1 ./play_bag.sh …  ← /motor_cmd까지 재생 (실제 모터가 움직일 수 있음)
#   - bags/<이름> 폴더가 없으면 bags/<이름>.tar.gz를 SHA256SUMS로 확인한 뒤 풀어서 재생
#   - 이름 없이 실행하면 재생할 수 있는 목록 출력
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BAG_DIR="$HERE/bags"

NAME="${1%/}"; NAME="${NAME%.tar.gz}"; NAME="${NAME#bags/}"
if [ -z "$NAME" ]; then
  echo "재생 가능한 bag:"
  (cd "$BAG_DIR" 2>/dev/null && ls -1d -- */ *.tar.gz 2>/dev/null | sed 's|/$||; s|\.tar\.gz$||' | grep -v '^_' | sort -u | sed 's/^/  /')
  echo "사용법: $0 <이름> [ros2 bag play 옵션...]"
  exit 1
fi
shift

if [ ! -f "$BAG_DIR/$NAME/metadata.yaml" ]; then
  [ -f "$BAG_DIR/$NAME.tar.gz" ] || { echo "bags/$NAME, bags/$NAME.tar.gz 모두 없음"; exit 1; }
  if grep -qE "  bags/$NAME\.tar\.gz$" "$HERE/SHA256SUMS" 2>/dev/null; then
    (cd "$HERE" && grep -E "  bags/$NAME\.tar\.gz$" SHA256SUMS | sha256sum -c -) || { echo "체크섬 불일치 — 파일 손상"; exit 1; }
  else
    echo "※ SHA256SUMS에 없는 파일 — 체크섬 확인 생략"
  fi
  echo "=== 압축 해제: bags/$NAME.tar.gz"
  tar -C "$BAG_DIR" -xzf "$BAG_DIR/$NAME.tar.gz"
fi

MODE_ARG=--local; [ "${LINK:-0}" = 1 ] && MODE_ARG=--link
# shellcheck disable=SC1091
source "$HERE/env.sh" "$MODE_ARG"

EXTRA=()
if [ "$LV2_MODE" = link ] && [ "${WITH_MOTOR:-0}" != 1 ]; then
  EXTRA=(--exclude-topics /motor_cmd)
  echo "※ 연동 모드: /motor_cmd 제외하고 재생 (포함하려면 WITH_MOTOR=1)"
fi
echo "=== ROS $ROS_DISTRO · $LV2_MODE 모드 · ROS_DOMAIN_ID=$ROS_DOMAIN_ID${ROS_STATIC_PEERS:+ · PEERS=$ROS_STATIC_PEERS}"
echo "=== 재생: bags/$NAME — 끝내려면 Ctrl+C"
exec ros2 bag play "$BAG_DIR/$NAME" "${EXTRA[@]}" "$@"
