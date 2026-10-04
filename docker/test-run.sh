#!/bin/bash
# test-run: 빌드 + bring-up + 토픽 덤프 (컨테이너 안에서 실행)
#   test-run [초]   (기본 10초, 0이면 Ctrl+C까지 무한 수집)
# 결과: /ws/debug/topic/{토픽명}/echo (계속 append), .../info (pub/sub 상세)
#       /ws/debug/topic/nodes.txt (노드 목록)
# 검사가 아니라 덤프용. CI에서는 5초짜리로 launch 검증 겸용.
# shellcheck disable=SC1091
source /opt/ros/lyrical/setup.bash
cd /ws || { echo "FAIL: /ws 없음"; exit 2; }

colcon build --symlink-install || { echo "FAIL: colcon build"; exit 1; }
source install/setup.bash

DUR=${1:-10}
OUT=/ws/debug/topic
rm -rf "$OUT"
mkdir -p "$OUT"

# 포트 모드 강제 (pyserial 등이 cooked로 바꿔놓고 가면 I/O가 깨짐)
stty -F /dev/ttyV0 raw -echo 2>/dev/null
stty -F /dev/ttyV1 raw -echo 2>/dev/null

cleanup() {
  kill "$LAUNCH_PID" "${ECHO_PIDS[@]}" 2>/dev/null
  # launch 래퍼가 죽어도 자식 노드가 고아로 남을 수 있어서 직접 정리
  pkill -f 'bringup_test/(talker|listener)' 2>/dev/null || true
  wait 2>/dev/null || true
}
trap cleanup INT TERM

# 이전 실행의 고아 노드가 있으면 DDS 그래프가 꼬이니 먼저 정리
pkill -f 'bringup_test/(talker|listener)' 2>/dev/null || true
sleep 1

ros2 launch bringup_test bringup.launch.py >/dev/null 2>&1 & LAUNCH_PID=$!
sleep 6  # DDS discovery 대기

ros2 node list > "$OUT/nodes" 2>&1
ros2 topic list > "$OUT/topics" 2>&1
echo "--- nodes ---"
cat "$OUT/nodes"
echo "--- topics ---"
cat "$OUT/topics"

ECHO_PIDS=()
for topic in $(ros2 topic list 2>/dev/null); do
  case "$topic" in
    /parameter_events|/rosout) continue ;;
  esac
  dir="$OUT/${topic#/}"
  mkdir -p "$dir"
  ros2 topic info -v "$topic" > "$dir/info" 2>&1
  if [ "$DUR" = 0 ] || [ "$DUR" = inf ]; then
    ros2 topic echo "$topic" >> "$dir/echo" 2>&1 &
  else
    timeout "$DUR" ros2 topic echo "$topic" >> "$dir/echo" 2>&1 &
  fi
  ECHO_PIDS+=($!)
done

if [ "$DUR" = 0 ] || [ "$DUR" = inf ]; then
  echo "수집 중... 종료는 Ctrl+C"
  wait "$LAUNCH_PID" 2>/dev/null || true
else
  sleep "$((DUR + 2))"
  cleanup
fi

echo "--- dump ---"
find "$OUT" -type f | sort
echo "DONE: test-run (${DUR}s)"
