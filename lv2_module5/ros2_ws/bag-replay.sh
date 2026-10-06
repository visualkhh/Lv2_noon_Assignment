#!/bin/bash
# rosbag 재생 — 그대로 재생(play) 또는 영상만 검출기에 다시 넣기(reprocess)
#
#   ./bag-replay.sh 20261006_153000_success               # 전체 토픽 재생 (RUN_ID 또는 bag 경로)
#   ./bag-replay.sh -t /target -t /tracking_status <bag>  # 고른 토픽만 재생
#   ./bag-replay.sh -r 0.5 -L <bag>                       # 0.5배속, 반복
#   ./bag-replay.sh --reprocess <bag>                     # 입력 재처리: 영상 → perception_node → /target_replay
#   ./bag-replay.sh --reprocess --record <bag>            # 재처리 결과를 <RUN_ID>_replay bag으로 기록
#
# 옵션
#   -t <토픽>        재생할 토픽 (여러 번). 없으면 bag의 전체 토픽
#   -r <배속>        재생 속도 (기본 1.0)
#   -L               반복 재생
#   --reprocess      bag의 영상만 재생하고 perception_node를 새로 돌려 /target_replay로 출력
#   -c <yaml>        --reprocess에서 쓸 perception 설정 (기본: 설치된 realsense.yaml)
#   -i <토픽>        --reprocess에서 쓸 영상 토픽 (기본 /camera/camera/color/image_raw)
#   --record         --reprocess 결과(/target_replay)를 bag으로 기록
#   --force          dynamixel_controller가 실행 중이어도 재생 (모터가 움직일 수 있음)
#
# 안전: 재생한 /motor_cmd·/target을 dynamixel_controller가 받으면 실제 모터가 움직인다.
#       controller가 실행 중이면 --force 없이는 재생하지 않는다.
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-lyrical}"

usage() { awk 'NR > 1 && (/^# NOTE/ || !/^#/) {exit} NR > 1' "$0"; }   # 맨 위 주석 = 도움말

topics=()
rate=1.0
loop=0
reprocess=0
record=0
force=0
config=''
image_topic=/camera/camera/color/image_raw
bag_arg=''
while (($#)); do
  case "$1" in
    -t) topics+=("$2"); shift 2 ;;
    -r) rate=$2; shift 2 ;;
    -L) loop=1; shift ;;
    -c) config=$2; shift 2 ;;
    -i) image_topic=$2; shift 2 ;;
    --reprocess) reprocess=1; shift ;;
    --record) record=1; shift ;;
    --force) force=1; shift ;;
    -h | --help) usage; exit 0 ;;
    -*) echo "모르는 옵션: $1"; usage; exit 2 ;;
    *) bag_arg=$1; shift ;;
  esac
done
[ -n "$bag_arg" ] || { usage; exit 2; }

# bag 위치: 경로 그대로 → recordings/<RUN_ID> 순서로 찾는다
bag=''
for candidate in "$bag_arg" "$HERE/../recordings/$bag_arg" "$HERE/recordings/$bag_arg"; do
  if [ -f "$candidate/metadata.yaml" ]; then
    bag="$(cd "$candidate" && pwd)"
    break
  fi
done
[ -n "$bag" ] || { echo "FAIL: bag 없음: $bag_arg (metadata.yaml이 있는 폴더 또는 recordings/의 RUN_ID)"; exit 1; }

# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
# shellcheck disable=SC1091
[ -f "$HERE/install/setup.bash" ] && source "$HERE/install/setup.bash"

if pgrep -f 'lib/dynamixel/dynamixel_controller' >/dev/null && [ "$force" = 0 ]; then
  echo "FAIL: dynamixel_controller 실행 중 → 재생한 명령으로 모터가 움직일 수 있음."
  echo "      start.sh 등을 먼저 종료하거나, 의도한 것이면 --force"
  exit 1
fi

play_args=(--rate "$rate")
[ "$loop" = 1 ] && play_args+=(--loop)

pids=()
cleanup() {
  # 백그라운드 프로세스는 SIGINT를 무시하므로 SIGTERM으로 끈다 (rosbag2·rclcpp 모두 정상 종료)
  for pid in "${pids[@]}"; do
    kill -TERM "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
}
trap cleanup EXIT
trap 'true' INT   # Ctrl+C는 ros2 bag play만 멈추고, 나머지는 cleanup이 정리

if [ "$reprocess" = 0 ]; then
  [ "${#topics[@]}" -gt 0 ] && play_args+=(--topics "${topics[@]}")
  echo "=== 재생: $bag"
  ros2 bag play "$bag" "${play_args[@]}" || true
  exit 0
fi

# --- 입력 재처리: 영상만 재생 → perception_node(bag 시각) → /target_replay
if [ -z "$config" ]; then
  config="$(ros2 pkg prefix realsense)/share/realsense/config/realsense.yaml"
fi
[ -f "$config" ] || { echo "FAIL: 설정 파일 없음: $config"; exit 1; }

echo "=== perception_node (설정 $config, 출력 /target_replay, use_sim_time)"
ros2 run realsense perception_node --ros-args \
  --params-file "$config" \
  -p image_topic:="$image_topic" \
  -p target_topic:=/target_replay \
  -p use_sim_time:=true &
pids+=($!)

if [ "$record" = 1 ]; then
  replay_bag="$(dirname "$bag")/$(basename "$bag")_replay"
  [ -e "$replay_bag" ] && { echo "FAIL: 이미 있음: $replay_bag"; exit 1; }
  echo "=== 기록: /target_replay → $replay_bag"
  ros2 bag record -o "$replay_bag" --topics /target_replay &
  pids+=($!)
fi

sleep 2   # 노드·구독 준비
echo "=== 재생 (영상만, --clock): $bag"
ros2 bag play "$bag" "${play_args[@]}" --topics "$image_topic" --clock || true
sleep 1   # 마지막 프레임 처리 대기
echo "=== 완료. 비교: 원본 /target vs 재처리 /target_replay"
if [ "$record" = 1 ]; then
  echo "    재처리 bag: $replay_bag"
fi
