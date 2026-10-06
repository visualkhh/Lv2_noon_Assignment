#!/bin/bash
# rosbag 기록 — 토픽 묶음(preset) 또는 토픽을 골라 기록하고, 끝나면 정보·크기·해시를 남긴다
#
#   ./bag-recording.sh success                       # core 토픽, Ctrl+C로 종료
#   ./bag-recording.sh -d 20 lost                    # 20초 후 자동 종료
#   ./bag-recording.sh -p all success                # 디버그 영상까지 전부
#   ./bag-recording.sh -t /target -t /motor_cmd test # 토픽만 직접 지정
#   ./bag-recording.sh -p core -t /perception_node/mask/compressed test   # preset + 토픽 추가
#   ./bag-recording.sh -l                            # preset·토픽 목록 보기
#
# 옵션
#   -p <preset>  core · control · camera · debug · all  (여러 번 쓰면 합침)
#   -t <토픽>     토픽 추가 (여러 번)
#                -p·-t 둘 다 없으면 core
#   -d <초>       기록 시간. 없으면 Ctrl+C까지
#   -o <폴더>     저장 위치 (기본: ../recordings, 없으면 ./recordings)
#   -s <형식>     저장 형식 (mcap·sqlite3, 기본: ros2 bag 기본값 = mcap)
#   -z           zstd 압축 (용량↓, CPU↑ — Raspberry Pi에선 영상 기록 시 메시지 누락 여부를 info로 확인)
#
# 용량: 원본 영상(640x480 rgb8 30fps)은 약 27MB/s → 30초에 약 800MB.
#       시작 전에 남은 공간을 확인하고, -d가 있으면 예상 용량보다 부족할 때 시작하지 않는다.
#
# 결과 (recordings/README.md 이름 규칙)
#   <폴더>/<RUN_ID>/            bag (metadata.yaml + 데이터 파일)
#   <폴더>/<RUN_ID>.info.txt    ros2 bag info 출력 (토픽·메시지 수·기간)
#   <폴더>/<RUN_ID>.sha256      체크섬
#   RUN_ID = YYYYMMDD_HHMMSS_<장면>
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-lyrical}"

IMAGE=/camera/camera/color/image_raw
CAMERA_INFO=/camera/camera/color/camera_info
declare -A PRESETS=(
  [core]="$IMAGE /target /motor_cmd /tracking_status"   # 과제 필수 (영상·목표·상태·명령)
  [control]="/target /motor_cmd /tracking_status"       # 영상 없이 가볍게 (긴 시험용)
  [camera]="$IMAGE $CAMERA_INFO"                        # 입력 영상만
  [debug]="/perception_node/debug_image/compressed /perception_node/mask/compressed"
  [all]="$IMAGE $CAMERA_INFO /target /motor_cmd /tracking_status /perception_node/debug_image/compressed /perception_node/mask/compressed"
)
PRESET_ORDER=(core control camera debug all)

usage() { awk 'NR > 1 && (/^# NOTE/ || !/^#/) {exit} NR > 1' "$0"; }   # 맨 위 주석 = 도움말

list_presets() {
  for p in "${PRESET_ORDER[@]}"; do
    printf '%-8s %s\n' "$p" "${PRESETS[$p]}"
  done
}

presets=()
topics=()
duration=''
out_dir=''
storage=''
compress=0
while getopts 'p:t:d:o:s:zlh' opt; do
  case "$opt" in
    p) [ -n "${PRESETS[$OPTARG]:-}" ] || { echo "모르는 preset: $OPTARG"; list_presets; exit 2; }
       presets+=("$OPTARG") ;;
    t) topics+=("$OPTARG") ;;
    d) [[ $OPTARG =~ ^[0-9]+$ ]] || { echo "-d는 초(정수)"; exit 2; }
       duration=$OPTARG ;;
    o) out_dir=$OPTARG ;;
    s) storage=$OPTARG ;;
    z) compress=1 ;;
    l) list_presets; exit 0 ;;
    h) usage; exit 0 ;;
    *) usage; exit 2 ;;
  esac
done
shift $((OPTIND - 1))
scene=${1:-run}
[[ $scene =~ ^[A-Za-z0-9_-]+$ ]] || { echo "장면 이름은 영문·숫자·_·- 만: $scene"; exit 2; }

# preset을 안 줬고 -t도 없으면 core
if [ "${#presets[@]}" = 0 ] && [ "${#topics[@]}" = 0 ]; then
  presets=(core)
fi
for p in "${presets[@]}"; do
  read -ra t <<< "${PRESETS[$p]}"
  topics+=("${t[@]}")
done
# 중복 제거 (순서 유지)
declare -A seen=()
unique=()
for t in "${topics[@]}"; do
  [ -n "${seen[$t]:-}" ] || unique+=("$t")
  seen[$t]=1
done
topics=("${unique[@]}")

if [ -z "$out_dir" ]; then
  if [ -d "$HERE/../recordings" ]; then out_dir="$HERE/../recordings"; else out_dir="$HERE/recordings"; fi
fi
mkdir -p "$out_dir"
out_dir="$(cd "$out_dir" && pwd)"

# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
# shellcheck disable=SC1091
[ -f "$HERE/install/setup.bash" ] && source "$HERE/install/setup.bash"

# 지금 발행 중인 토픽인지 확인 (없어도 기록은 시작 — 나중에 뜨면 잡힘)
live=$(ros2 topic list 2>/dev/null || true)
for t in "${topics[@]}"; do
  if grep -qxF -- "$t" <<< "$live"; then
    echo "ok:   $t"
  else
    echo "WARN: $t (지금 발행 중이 아님)"
  fi
done

# 디스크 확인 — 원본 영상이 있으면 약 27MB/s (실측 4.8초 127MB)
IMAGE_MBPS=28
free_mb=$(df -Pm "$out_dir" | awk 'NR == 2 {print $4}')
has_image=0
for t in "${topics[@]}"; do [ "$t" = "$IMAGE" ] && has_image=1; done
if [ "$has_image" = 1 ]; then
  echo "용량: 원본 영상 약 ${IMAGE_MBPS}MB/s (압축 전) · 남은 공간 ${free_mb}MB → 약 $((free_mb / IMAGE_MBPS))초 기록 가능"
  if [ -n "$duration" ] && [ "$compress" = 0 ]; then
    need=$((duration * IMAGE_MBPS * 12 / 10))   # 20% 여유
    [ "$free_mb" -ge "$need" ] || { echo "FAIL: 공간 부족 (필요 약 ${need}MB, 남은 ${free_mb}MB). -z(압축)·짧은 -d·-p control 사용"; exit 1; }
  fi
fi
[ "$free_mb" -ge 500 ] || echo "WARN: 남은 공간 ${free_mb}MB — 기록 중 가득 찰 수 있음"

run_id="$(date +%Y%m%d_%H%M%S)_$scene"
bag="$out_dir/$run_id"
args=(-o "$bag")
[ -n "$storage" ] && args+=(-s "$storage")
if [ "$compress" = 1 ]; then
  if [ -z "$storage" ] || [ "$storage" = mcap ]; then
    args+=(--storage-preset-profile zstd_fast)          # mcap 청크 압축
  else
    args+=(--compression-mode file --compression-format zstd)
  fi
fi

echo "=== 기록: $bag"
[ -n "$duration" ] && echo "    ${duration}초 후 자동 종료" || echo "    Ctrl+C로 종료"

# 기록기는 백그라운드로 띄운다. 비대화형 bash의 백그라운드 프로세스는 SIGINT를 무시하므로
# 멈출 때는 SIGTERM을 보낸다 (rosbag2는 SIGTERM에도 정상 종료하고 metadata.yaml을 쓴다).
log=$(mktemp)
ros2 bag record "${args[@]}" --topics "${topics[@]}" > >(tee "$log") 2>&1 &
rec_pid=$!
trap 'kill -TERM "$rec_pid" 2>/dev/null || true' INT    # Ctrl+C → 기록 종료 후 정리 단계로
if [ -n "$duration" ]; then
  # ros2 bag record에는 "N초 후 종료" 옵션이 없다(-d는 파일 분할). 시작·토픽 구독까지 몇 초 걸리므로
  # 실제 기록이 시작된 뒤부터 시간을 잰다: "Recording..." → 모든 토픽 구독(최대 5초 더)
  for _ in $(seq 100); do grep -q 'Recording\.\.\.' "$log" && break; sleep 0.1; done
  for _ in $(seq 50); do grep -q 'All requested topics are subscribed' "$log" && break; sleep 0.1; done
  sleep "$duration" &
  wait $! || true                        # Ctrl+C면 바로 깨어남
  kill -TERM "$rec_pid" 2>/dev/null || true
fi
status=0
while kill -0 "$rec_pid" 2>/dev/null; do
  wait "$rec_pid" || status=$?          # Ctrl+C로 wait가 끊겨도 기록기가 끝날 때까지 기다림
done
rm -f "$log"
[ "$status" = 143 ] && status=0          # SIGTERM 종료는 정상
trap - INT

[ -f "$bag/metadata.yaml" ] || { echo "FAIL: bag이 만들어지지 않음 ($bag, exit $status)"; exit 1; }

echo "=== 정리"
ros2 bag info "$bag" | tee "$out_dir/$run_id.info.txt"
(cd "$out_dir" && sha256sum "$run_id"/* > "$run_id.sha256")
echo
echo "RUN_ID : $run_id"
echo "bag    : $bag ($(du -sh "$bag" | cut -f1))"
echo "info   : $out_dir/$run_id.info.txt"
echo "sha256 : $out_dir/$run_id.sha256"
echo "재생   : ./bag-replay.sh $run_id"
