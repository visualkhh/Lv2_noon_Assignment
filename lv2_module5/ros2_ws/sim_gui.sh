#!/bin/bash
# 웹 GUI + run_and_record.sh
#   ① index.html을 http://localhost:8000 으로 제공
#      (VS Code "Browser: Open Integrated Browser" → http://localhost:8000/index.html)
#   ② ../recordings/run_and_record.sh 를 그대로 실행
#      빌드 → 노드 실행(기기가 없으면 가상, RealSense·OpenCR를 꽂으면 자동 실기 전환) → 녹화
#      → Ctrl+C → 압축·SHA256SUMS·README [목록] 등록
#   ③ 끝나면 GUI 웹 서버 종료
#
#   ./sim_gui.sh                       # 이름 sim_YYYYmmdd_HHMMSS로 실행·녹화
#   ./sim_gui.sh use_devices:=false    # 인자는 run_and_record.sh로 그대로 전달 (이름·launch 인자)
#   NO_RECORD=1 ./sim_gui.sh           # 녹화 안 함
#   RECORD_RAW=1 ./sim_gui.sh          # 원본 영상(image_raw, 약 13MB/s)까지 녹화 (기본은 디버그 영상)
#   LINK=1 ./sim_gui.sh                # connect.sh로 연동한 상대 기기와 같은 네트워크에서 실행
# ROS 배포판은 기기마다 자동 탐지 (../recordings/env.sh), 처음 한 번은 ../recordings/connect.sh로 점검
# 최초 1회: rosbridge_suite 필요 (apt 배포판: sudo apt install ros-<배포판>-rosbridge-suite, 소스 빌드 배포판은 소스 빌드)
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
GUI="$HERE/../index.html"
HTTP_PORT="${HTTP_PORT:-8000}"

# shellcheck disable=SC1091
source "$HERE/../recordings/env.sh" --local
ros2 pkg prefix rosbridge_server >/dev/null 2>&1 ||
  echo "WARN: rosbridge_server 없음 → GUI가 연결되지 않음 ($ROS_DISTRO용 rosbridge_suite 설치 필요)"

# --- ① GUI 웹 서버 (VS Code Integrated Browser는 file:// 대신 http 주소로 연다)
python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 --directory "$(dirname "$GUI")" >/dev/null 2>&1 &
HTTP_PID=$!
trap 'kill $HTTP_PID 2>/dev/null' EXIT
echo "=== GUI: http://localhost:$HTTP_PORT/index.html"
echo "    VS Code: Ctrl+Shift+P → Browser: Open Integrated Browser → 위 주소 입력"

# --- ② run_and_record.sh (Ctrl+C는 그쪽에서 녹화 종료·등록으로 처리, 여기는 끝날 때까지 대기)
trap ':' INT
RECORD_RAW="${RECORD_RAW:-0}" SCENE_DESC="${SCENE_DESC:-sim_gui 자동 녹화}" \
  "$HERE/../recordings/run_and_record.sh" "$@" || true
