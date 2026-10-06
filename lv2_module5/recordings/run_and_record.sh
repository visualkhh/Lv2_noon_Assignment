#!/usr/bin/env bash
# 사용법: ./run_and_record.sh [이름] [launch 인자...]
#   ./run_and_record.sh                          ← sim_YYYYmmdd_HHMMSS 이름으로 실행·녹화
#   ./run_and_record.sh scene5                   ← 이름 지정
#   ./run_and_record.sh scene5 use_tracker:=false ← launch 인자는 sim.launch.py로 그대로 전달
#   NO_RECORD=1 ./run_and_record.sh              ← 실행만 (녹화·등록 안 함)
#   RECORD_RAW=0 ./run_and_record.sh             ← 원본 영상 대신 디버그 영상(jpeg)으로 가볍게 녹화
#   SCENE_DESC="..." ./run_and_record.sh         ← README '장면' 칸 (없으면 끝날 때 입력받음)
#
# 순서: ① 빌드 → ② sim.launch.py 실행 (기기가 없으면 가상, RealSense·OpenCR를 꽂으면 자동 실기 전환)
#       → ③ 토픽 대기 → ④ 녹화 (Ctrl+C로 종료) → ⑤ 노드 종료
#       → ⑥ pack_bag.sh로 압축·SHA256SUMS → ⑦ README.md [목록] 등록 (카메라·OpenCR 상태 자동 기입)
# sim_gui.sh는 GUI 웹 서버를 띄운 뒤 이 스크립트를 그대로 실행한다.
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

# ===== 설정 =====
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
WAIT_TOPICS=(/target /motor_cmd /tracking_status)                 # 뜰 때까지 기다리는 토픽
REC_TOPICS=(/target /motor_cmd /tracking_status /camera_source /opencr_status
            /joint_states /virtual_target)                        # 상태·제어·3D
if [ "${RECORD_RAW:-1}" = 1 ]; then
  REC_TOPICS+=(/camera/camera/color/image_raw)                    # 원본 영상 (약 13MB/s)
else
  REC_TOPICS+=(/perception_node/debug_image/compressed)           # 디버그 영상 (jpeg)
fi
WAIT_SEC=60                         # 노드 기동 대기 최대 시간
# ================

set -eo pipefail
# Ctrl+C는 오류가 아니라 정상 종료로 처리한다
#   녹화 전(빌드·토픽 대기): 녹화 없이 종료 (exit 0)
#   녹화 중: 녹화만 멈추고 압축·등록까지 진행
#   녹화 후(압축·등록): 무시 — 중간에 끊겨 bag·README가 반쯤 써지는 것을 막음
trap 'echo; echo "=== 녹화 전에 중단 (Ctrl+C) — 녹화·등록 없이 종료"; exit 0' INT
HERE="$(cd "$(dirname "$0")" && pwd)"
WS="$(cd "$HERE/../ros2_ws" && pwd)"

# 이름: ':='가 없는 첫 인자 > sim_YYYYmmdd_HHMMSS
if [ -n "$1" ] && [[ "$1" != *:=* ]]; then
  NAME="${1%/}"; shift
else
  NAME="sim_$(date +%Y%m%d_%H%M%S)"
fi
[ -e "$HERE/$NAME" ] && { echo "$NAME 폴더가 이미 있음 — 다른 이름을 주세요"; exit 1; }

# --- ① 빌드
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
(cd "$WS" && colcon build --symlink-install --packages-select cognitive_control)
# shellcheck disable=SC1091
source "$WS/install/setup.bash"

# --- ② 노드 실행 (별도 프로세스 그룹: 녹화 중 Ctrl+C가 launch까지 죽이지 않도록)
LOG="${TMPDIR:-/tmp}/$NAME.launch.log"
echo "=== ros2 launch cognitive_control sim.launch.py $* (로그: $LOG)"
setsid ros2 launch cognitive_control sim.launch.py "$@" >"$LOG" 2>&1 &
SIM=$!

stop_sim() {
  # launch가 먼저 끝나도 rosbridge 등 자식이 남을 수 있어 프로세스 그룹 전체가 사라질 때까지 확인한다
  # (남으면 9090 포트를 잡고 있어 다음 실행의 GUI 연결이 꼬인다)
  group_alive() { pgrep -g "$SIM" >/dev/null 2>&1; }
  group_alive || return 0
  echo "=== 노드 종료"
  local sig
  for sig in INT TERM KILL; do
    kill -"$sig" -- -"$SIM" 2>/dev/null || true
    for _ in $(seq 10); do group_alive || return 0; sleep 0.5; done
  done
}
trap stop_sim EXIT

if [ "${NO_RECORD:-0}" = 1 ]; then
  echo "=== 녹화 없이 실행 중 — 끝내려면 Ctrl+C"
  trap ':' INT
  wait "$SIM" || true
  exit 0
fi

# --- ③ 토픽 대기
echo "=== 토픽 대기 (최대 ${WAIT_SEC}s): ${WAIT_TOPICS[*]}"
for ((t = 0; t < WAIT_SEC; t += 2)); do
  if ! kill -0 "$SIM" 2>/dev/null; then
    echo "FAIL: launch가 종료됨 — 로그 끝부분:"; tail -30 "$LOG"; exit 1
  fi
  LIST=$(ros2 topic list 2>/dev/null || true)
  MISSING=()
  for tp in "${WAIT_TOPICS[@]}"; do grep -qx "$tp" <<<"$LIST" || MISSING+=("$tp"); done
  [ ${#MISSING[@]} -eq 0 ] && break
  sleep 2
done
if [ ${#MISSING[@]} -gt 0 ]; then
  echo "⚠ 시간 안에 안 뜬 토픽: ${MISSING[*]} (노드 실행·토픽 이름 확인, 로그: $LOG)"
  if [ -t 0 ]; then
    read -rp "그래도 녹화할까요? (y/N): " ans
    [[ "$ans" == y || "$ans" == yes ]] || { echo "중단"; exit 1; }
  fi
fi

# --- ④ 녹화 (Ctrl+C는 녹화만 멈추고 이 스크립트는 등록까지 계속)
echo "=== 녹화 시작: recordings/$NAME (${REC_TOPICS[*]}) — 끝내려면 Ctrl+C"
trap ':' INT
(cd "$HERE" && ros2 bag record -o "$NAME" "${REC_TOPICS[@]}") || true

trap '' INT
echo "=== 녹화 종료 — 압축·등록 중 (이제 Ctrl+C는 무시됨)"

# --- ⑤ 노드 종료
stop_sim

# --- ⑥ 압축·SHA256SUMS (폴더가 있으므로 pack_bag.sh는 녹화 없이 압축·체크섬만)
[ -f "$HERE/$NAME/metadata.yaml" ] || { echo "bag 폴더 없음 또는 손상: $NAME"; exit 1; }
"$HERE/pack_bag.sh" "$NAME"

# --- ⑦ README.md [목록] 등록
cd "$HERE"
FILE="${NAME}.tar.gz"
INFO=$(ros2 bag info "$NAME" 2>/dev/null || true)

# 기간: "Duration: 10.93166s" → 10.9s
DUR=$(awk '/Duration:/ {gsub("s","",$2); printf "%.1fs", $2}' <<<"$INFO")

# 토픽: ROS 기본 토픽 제외, 메시지 1개 이상만 "이름(개수)"
TOPIC_COL=$(grep -oE "Topic: [^ ]+ \| Type: [^ ]+ \| Count: [0-9]+" <<<"$INFO" \
  | awk '{print $2, $8}' \
  | grep -vE "^/(rosout|parameter_events|events/write_split) " \
  | awk '$2 > 0 {printf "%s%s(%s)", sep, $1, $2; sep=", "}' || true)

if [ -z "$TOPIC_COL" ]; then
  echo "⚠ 실제 데이터 토픽이 없는 bag입니다."
  ans=n
  [ -t 0 ] && read -rp "그래도 README에 등록할까요? (y/N): " ans
  [[ "$ans" == y || "$ans" == yes ]] || { echo "README 등록 생략"; exit 0; }
  TOPIC_COL="(데이터 없음)"
fi

DESC="${SCENE_DESC:-}"
if [ -z "$DESC" ] && [ -t 0 ]; then
  read -rp "장면 설명 (README '장면' 칸, 엔터=생략): " DESC
fi
# 녹화 중 카메라 출처·OpenCR 연결 상태 (바뀌었으면 virtual→realsense 처럼)
SOURCES=$(python3 - "$NAME" <<'PY' 2>/dev/null || true
import sys
from rclpy.serialization import deserialize_message
import rosbag2_py
from std_msgs.msg import String
reader = rosbag2_py.SequentialReader()
reader.open(rosbag2_py.StorageOptions(uri=sys.argv[1]), rosbag2_py.ConverterOptions('', ''))
seen = {'/camera_source': [], '/opencr_status': []}
while reader.has_next():
    topic, data, _ = reader.read_next()
    if topic in seen:
        v = deserialize_message(data, String).data.split(' ')[0]
        if v not in seen[topic]:
            seen[topic].append(v)
if any(seen.values()):
    print(f"카메라: {'→'.join(seen['/camera_source']) or '-'}, "
          f"OpenCR: {'→'.join(seen['/opencr_status']) or '-'}")
PY
)
[ -n "$SOURCES" ] && DESC="${DESC:+$DESC }($SOURCES)"
DESC="${DESC:--}"

SIZE=$(du -h "$FILE" | cut -f1)
SHA=$(sha256sum "$FILE" | cut -d" " -f1)
COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "-")

ENTRY="[목록:$FILE]
- 장면: $DESC
- 기간: $DUR
- 토픽: $TOPIC_COL
- 용량: $SIZE
- SHA256: \`$SHA\`
- 기준 커밋: \`$COMMIT\`
- 다운로드: (업로드 후 기입)"

# 같은 파일 항목은 교체, 없으면 파일 끝에 추가 ("등록된 bag 없음" 줄은 제거)
python3 - "$FILE" "$ENTRY" <<'PY'
import sys
f, entry = sys.argv[1], sys.argv[2]
lines = open("README.md", encoding="utf-8").read().rstrip("\n").split("\n")
lines = [l for l in lines if not l.startswith("- 등록된 bag 없음")]
head = f"[목록:{f}]"
if head in lines:
    a = lines.index(head)
    b = a + 1
    while b < len(lines) and lines[b].startswith("- "):
        b += 1
    lines[a:b] = entry.split("\n")
else:
    if "[목록]" not in lines:
        lines += ["", "[목록]"]
    lines += [""] + entry.split("\n")
open("README.md", "w", encoding="utf-8").write("\n".join(lines) + "\n")
PY
echo "---- README.md 등록 ----"
echo "$ENTRY"
