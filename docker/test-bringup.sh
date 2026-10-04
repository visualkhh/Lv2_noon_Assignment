#!/bin/bash
# bring-up 검사 (컨테이너 안에서 실행): test-bringup
#   1. colcon 빌드
#   2. 실기 구성 그대로 bringup launch (카메라·모터 켠 기본값)
#   3. 실기 bringup의 노드가 전부 떴으면 PASS (launch·패키지 연결 검증)
# 실행 중엔 test-logger를 같이 띄워 /ws/debug/topic 에 토픽 echo를 기록한다.
# 컨테이너엔 RealSense가 없어서 영상은 안 나옴 → 데이터 흐름은 test-fake_camera_bringup에서 검사.
# NOTE: set -u 사용 금지 — setup.bash가 미설정 변수를 참조해서 죽음.
# shellcheck disable=SC1091
source /opt/ros/lyrical/setup.bash
cd /ws || { echo "FAIL: /ws 없음"; exit 2; }

colcon build --symlink-install || { echo "FAIL: colcon build"; exit 1; }
# shellcheck disable=SC1091
source install/setup.bash

NODES="/camera/camera /perception_node /dynamixel_move_node /dynamixel_controller"
KILL_PAT='lib/(fake_camera_bringup|realsense|realsense2_camera|dynamixel)/'

# 이전 실행의 고아 노드가 있으면 DDS 그래프가 꼬이니 먼저 정리
pkill -f "$KILL_PAT" 2>/dev/null || true

ros2 launch bringup bringup.launch.py & LAUNCH_PID=$!
# 띄워 둔 동안 토픽 기록 → 호스트 lv2_module5/debug/topic 에서 확인 (통제실 토픽 패널)
test-logger 0 > /tmp/test-logger.log 2>&1 & LOGGER_PID=$!
sleep 8  # 노드 기동 + DDS discovery 대기
UP=$(ros2 node list 2>/dev/null)
# logger 먼저 종료 (TERM → 루프 끝내고 echo 정리) 후 launch 종료
kill "$LOGGER_PID" 2>/dev/null
timeout 10 tail --pid="$LOGGER_PID" -f /dev/null
# NOTE: 비대화형 셸의 백그라운드 작업은 SIGINT가 무시됨 → TERM으로 종료
kill "$LAUNCH_PID" 2>/dev/null
timeout 10 tail --pid="$LAUNCH_PID" -f /dev/null
pkill -f "$KILL_PAT" 2>/dev/null || true

echo "--- nodes ---"
echo "$UP"
for n in $NODES; do
  echo "$UP" | grep -qx "$n" || { echo "FAIL: 노드 없음 $n"; exit 1; }
done
echo "토픽 기록: /ws/debug/topic (호스트 lv2_module5/debug/topic)"
echo "PASS: bringup (실기 구성 노드 전부 기동)"
