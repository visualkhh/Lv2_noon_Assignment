#!/bin/bash
# echo-all: 현재 떠 있는 모든 토픽을 토픽명별 파일로 echo 기록 (Ctrl+C로 종료)
#   echo-all.sh [초]   (기본 0 = Ctrl+C까지 계속)
# 결과: lv2_module5/ros2_ws/debug/echo/{토픽명, / → __}.txt  (append)
# 중간에 새로 생긴 토픽도 2초마다 확인해서 추가로 기록한다.
# shellcheck disable=SC1091
source /opt/ros/lyrical/setup.bash
# shellcheck disable=SC1091
[ -f /home/pa10/Lv2_noon_Assignment/lv2_module5/ros2_ws/install/setup.bash ] && source /home/pa10/Lv2_noon_Assignment/lv2_module5/ros2_ws/install/setup.bash

# launch가 Docker 컨테이너(ipc 분리)에 있으면 SHM으로는 데이터가 안 옴 → UDP 강제
export FASTDDS_BUILTIN_TRANSPORTS=${FASTDDS_BUILTIN_TRANSPORTS:-UDPv4}

DUR=${1:-0}
OUT=/home/pa10/Lv2_noon_Assignment/lv2_module5/ros2_ws/debug/echo
mkdir -p "$OUT"

STOP=0
trap 'STOP=1' INT TERM
declare -A PIDS
END=$((SECONDS + DUR))

echo "기록 위치: $OUT  (종료: Ctrl+C)"
while [ "$STOP" = 0 ]; do
  for topic in $(ros2 topic list 2>/dev/null | grep -vxE '/parameter_events|/rosout'); do
    [ -n "${PIDS[$topic]}" ] && continue
    file="$OUT/$(echo "${topic#/}" | sed 's#/#__#g').txt"
    # --truncate-length: 이미지 등 긴 배열은 16개까지만 → 파일이 수백 MB로 안 커짐
    ros2 topic echo --truncate-length 16 "$topic" >> "$file" 2>&1 &
    PIDS[$topic]=$!
    echo "+ $topic → $(basename "$file")"
  done
  [ "$DUR" != 0 ] && [ "$SECONDS" -ge "$END" ] && break
  sleep 2
done

kill "${PIDS[@]}" 2>/dev/null
sleep 1
kill -9 "${PIDS[@]}" 2>/dev/null
wait 2>/dev/null
echo "--- 결과 ---"
ls -la "$OUT"
