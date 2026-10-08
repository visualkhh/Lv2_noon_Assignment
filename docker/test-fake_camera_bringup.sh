#!/bin/bash
# bring-up 검사 (컨테이너 안에서 실행): test-fake_camera_bringup
#   1. colcon 빌드
#   2. fake_camera_bringup launch = bringup(인지+제어) + 더미 카메라(통제실 가상 카메라 장면 → 영상 토픽)
#   3. 영상 → perception → /target 까지 오면 PASS. 모터 명령(serial-out)은 참고로만
#      (장면은 사람이 정함 → 기둥이 가운데(데드밴드 안)면 dynamixel이 명령을 안 보내는 게 정상)
#   통제실(docker/test-controller/run-controller.py)이 debug/input-live/frame.png를 그려 줘야 영상이 나옴.
#   test-fake_camera_bringup [초] [간격]
#     초:   /target 첫 수신 후 더 돌릴 시간 (기본 2초, 0이면 Ctrl+C까지 → 끄면 판정)
#     간격: 프레임 발행 간격 period_s [s] (생략하면 launch 기본 0.033 = 30fps, RealSense처럼)
#           0.5초(target_timeout)보다 길면 매 프레임 LOST → 모터 명령 항상 0
#   예) test-fake_camera_bringup 0
# 실행 중엔 test-logger를 같이 띄워 /ws/debug/topic 에 토픽 echo를 기록한다.
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

# 이전 실행의 고아 노드가 있으면 DDS 그래프가 꼬이니 먼저 정리
pkill -f 'lib/(fake_camera_bringup|realsense|realsense2_camera|dynamixel)/' 2>/dev/null || true
: > /ws/debug/serial-out

DUR=${1:-2}
LAUNCH_ARGS=()
[ -n "$2" ] && LAUNCH_ARGS+=("period_s:=$2")
[ -e /ws/debug/input-live/frame.png ] \
  || echo "참고: /ws/debug/input-live/frame.png 없음 — 통제실(run-controller.py)을 켜고 [영상 송출]을 켜야 영상이 나옴"
# job control: 백그라운드 작업이 SIGINT를 무시하지 않게 (비대화형 셸 기본은 무시 → launch에 상속됨).
# ros2 launch는 SIGINT로 정상 종료한다 (노드에 SIGINT → 대기 → 필요 시 TERM·KILL).
# TERM으로 끄면 launch만 바로 죽고 노드가 남았다가, 끊긴 출력 파이프에 로그를 쓰는 순간 SIGPIPE로
# 정리 없이 죽음 (fake_camera가 fake-camera.png를 못 지우는 등).
set -m
ros2 launch fake_camera_bringup fake_camera_bringup.launch.py "${LAUNCH_ARGS[@]}" & LAUNCH_PID=$!
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
# launch 정상 종료 (SIGINT) — 노드 정리까지 최대 15초 대기
kill -INT "$LAUNCH_PID" 2>/dev/null
timeout 15 tail --pid="$LAUNCH_PID" -f /dev/null
pkill -f 'lib/(fake_camera_bringup|realsense|realsense2_camera|dynamixel)/' 2>/dev/null || true

[ -n "$TARGET" ] || { echo "FAIL: /target 수신 없음 (camera → perception 끊김)"; exit 1; }
echo "--- /target ---"
echo "$TARGET"
[ -s /ws/debug/serial-out ] \
  || echo "참고: serial-out 비어 있음 — 기둥이 화면 가운데면 정상 (통제실에서 기둥을 옆으로 옮겨 보세요)"
echo "--- serial-out (마지막 5줄) ---"
tail -n 5 /ws/debug/serial-out
echo "토픽 기록: /ws/debug/topic (호스트 lv2_module5/debug/topic)"
echo "PASS: fake_camera_bringup (fake camera → perception → dynamixel → serial)"
