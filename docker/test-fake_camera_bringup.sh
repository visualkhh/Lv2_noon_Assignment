#!/bin/bash
# bring-up 검사 (컨테이너 안에서 실행): test-fake_camera_bringup
#   1. colcon 빌드
#   2. fake_camera_bringup launch = bringup(인지+제어) + 더미 카메라(/ws/debug/input-images → 영상 토픽)
#   3. 더미 영상 → perception → /target → dynamixel → 가상 시리얼까지 왔으면 PASS
#      (/target 수신 + serial-out에 모터 명령 "V <pan> <tilt>" 확인)
#   test-fake_camera_bringup [초] [간격]
#     초:   /target 첫 수신 후 더 돌릴 시간 (기본 2초, 0이면 Ctrl+C까지)
#     간격: 더미 카메라 이미지 발행 간격 period_s [s] (생략하면 launch 기본 0.1)
#           0.5초(target_timeout)보다 길면 매 장면 LOST → 모터 명령 항상 0
#   /target 첫 수신 후 [초]만큼 더 돌리고 종료 → 그다음 판정. 통제실(run-controller.py)로 볼 땐 0.
# 실행 중엔 test-logger를 같이 띄워 /ws/debug/topic 에 토픽 echo를 기록한다.
# input-images가 비어 있으면 realsense 테스트 이미지(실제 기둥 장면)를 1.png… 로 채움.
# NOTE: set -u 사용 금지 — setup.bash가 미설정 변수를 참조해서 죽음.
# shellcheck disable=SC1091
source /opt/ros/lyrical/setup.bash
cd /ws || { echo "FAIL: /ws 없음"; exit 2; }

colcon build --symlink-install || { echo "FAIL: colcon build"; exit 1; }
# shellcheck disable=SC1091
source install/setup.bash

[ -e /dev/ttyACM0 ] || { echo "FAIL: /dev/ttyACM0 없음 (브릿지 미구동)"; exit 2; }
# 포트 모드 강제 (pyserial 등이 cooked로 바꿔놓고 가면 I/O가 깨짐)
stty -F /dev/ttyV0 raw -echo 2>/dev/null
stty -F /dev/ttyV1 raw -echo 2>/dev/null

IMG_DIR=/ws/debug/input-images
mkdir -p "$IMG_DIR"
if [ -z "$(ls -A "$IMG_DIR")" ]; then
  i=1
  for f in /ws/src/realsense/test/data/*.png; do
    cp "$f" "$IMG_DIR/$i.png"
    i=$((i + 1))
  done
  echo "input-images 비어 있음 → realsense 테스트 이미지 $((i - 1))장으로 채움"
fi

# 이전 실행의 고아 노드가 있으면 DDS 그래프가 꼬이니 먼저 정리
pkill -f 'lib/(fake_camera_bringup|realsense|realsense2_camera|dynamixel)/' 2>/dev/null || true
: > /ws/debug/serial-out

DUR=${1:-2}
PERIOD_ARG=()
[ -n "$2" ] && PERIOD_ARG=("period_s:=$2")
ros2 launch fake_camera_bringup fake_camera_bringup.launch.py "${PERIOD_ARG[@]}" & LAUNCH_PID=$!
# 띄워 둔 동안 토픽 기록 → 호스트 lv2_module5/debug/topic 에서 확인 (통제실 토픽 패널)
test-logger 0 > /tmp/test-logger.log 2>&1 & LOGGER_PID=$!
# 타입을 명시해야 /target이 아직 안 생겼을 때 바로 끝나지 않고 생길 때까지 기다림
TARGET=$(timeout 20 ros2 topic echo --once /target geometry_msgs/msg/PointStamped 2>/dev/null)
if [ "$DUR" = 0 ] || [ "$DUR" = inf ]; then
  echo "실행 중... 종료는 Ctrl+C (종료 후 판정)"
  # Ctrl+C는 이 스크립트만 받음(launch는 백그라운드라 SIGINT 무시) → wait만 깨고 아래 정리로 진행
  trap 'echo' INT
  wait "$LAUNCH_PID" 2>/dev/null
  # 이후 Ctrl+C는 무시 — 정리 도중 또 누르면 스크립트만 죽고 노드·logger가 고아로 남음
  trap '' INT
else
  sleep "$DUR"
fi
# logger 먼저 종료 (TERM → 루프 끝내고 echo 정리) 후 launch 종료
kill "$LOGGER_PID" 2>/dev/null
timeout 10 tail --pid="$LOGGER_PID" -f /dev/null
# NOTE: 비대화형 셸의 백그라운드 작업은 SIGINT가 무시됨 → TERM으로 종료
kill "$LAUNCH_PID" 2>/dev/null
timeout 10 tail --pid="$LAUNCH_PID" -f /dev/null
pkill -f 'lib/(fake_camera_bringup|realsense|realsense2_camera|dynamixel)/' 2>/dev/null || true

[ -n "$TARGET" ] || { echo "FAIL: /target 수신 없음 (camera → perception 끊김)"; exit 1; }
echo "--- /target ---"
echo "$TARGET"
grep -q "^V " /ws/debug/serial-out \
  || { echo "FAIL: serial-out에 모터 명령(V ...) 없음 (dynamixel → serial 끊김)"; exit 1; }
echo "--- serial-out (마지막 5줄) ---"
tail -n 5 /ws/debug/serial-out
echo "토픽 기록: /ws/debug/topic (호스트 lv2_module5/debug/topic)"
echo "PASS: fake_camera_bringup (fake camera → perception → dynamixel → serial)"
