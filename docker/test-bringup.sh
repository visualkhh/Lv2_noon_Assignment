#!/bin/bash
# bring-up 검사 (컨테이너 안에서 실행): test-bringup
#   1. colcon 빌드 → 2. talker→listener→가상시리얼 왕복 → serial-out에 쌓이면 PASS
# NOTE: set -u 사용 금지 — setup.bash가 미설정 변수를 참조해서 죽음.
# shellcheck disable=SC1091
source /opt/ros/lyrical/setup.bash
cd /ws || { echo "FAIL: /ws 없음"; exit 2; }

colcon build --symlink-install || { echo "FAIL: colcon build"; exit 1; }
# shellcheck disable=SC1091
source install/setup.bash

[ -e /dev/ttyV0 ] || { echo "FAIL: /dev/ttyV0 없음 (브릿지 미구동)"; exit 2; }
# 포트 모드 강제 (pyserial 등이 cooked로 바꿔놓고 가면 I/O가 깨짐)
stty -F /dev/ttyV0 raw -echo 2>/dev/null
stty -F /dev/ttyV1 raw -echo 2>/dev/null
: > /ws/debug/serial-out

ros2 run bringup_test talker & TALKER_PID=$!
ros2 run bringup_test listener & LISTENER_PID=$!
sleep 8
kill "$TALKER_PID" "$LISTENER_PID" 2>/dev/null
wait 2>/dev/null || true

grep -q "bringup hello" /ws/debug/serial-out \
  || { echo "FAIL: serial-out에 bring-up 메시지 없음"; exit 1; }
echo "--- serial-out ---"
cat /ws/debug/serial-out
echo "PASS: bringup (talker→listener→serial)"
