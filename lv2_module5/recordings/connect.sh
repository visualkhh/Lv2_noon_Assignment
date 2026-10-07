#!/usr/bin/env bash
# 연동 스크립트 — 어떤 기기에서든 이 파일 하나만 실행하면 그 기기의 ROS 환경에 맞춰 설정하고 상대 기기와 연결한다
#
# 사용법
#   ./connect.sh                          ← 저장된 설정으로 (처음이면 물어봄) 점검·빌드·연동
#   ./connect.sh agumon.local             ← 상대 주소 지정 (여러 대: ./connect.sh 192.168.0.10 agumon.local)
#                                         ROS_DOMAIN_ID는 상대가 노드를 띄운 도메인을 자동으로 찾아 맞춤
#   ./connect.sh agumon.local --domain 63 ← 자동 탐색 대신 도메인 지정 (그 도메인에 상대 노드가 있는지만 확인)
#   ./connect.sh --local                  ← 상대 없이 이 PC만 점검·빌드
#   ./connect.sh --docker <컨테이너>      ← 호스트에 ROS가 없고 Docker 컨테이너에 있을 때: 이 폴더를 넣고 그 안에서 실행
#   옵션: --ws <워크스페이스 경로>  --ros <배포판 또는 setup.bash 경로>  --no-build  --yes(묻지 않음)
#   zip으로 받아 실행 권한이 없으면: bash connect.sh
#
# 하는 일
#   ① 기기 정보 (OS·CPU·Python·Docker 안인지)
#   ② ROS 2 배포판 자동 탐지 (lyrical·kilted·jazzy·humble·소스 빌드) + 필수/선택 패키지 점검
#   ③ cognitive_control 워크스페이스 찾기 → 이 기기 배포판으로 빌드 (없으면 녹화·재생 전용 모드)
#   ④ VS Code가 이 기기의 ROS를 보도록 경로 연결 (저장소 안일 때)
#   ⑤ 상대 기기 연결: 주소 확인 → ping → 상대가 노드를 띄운 ROS_DOMAIN_ID 자동 탐색 → 토픽 확인
#   ⑥ 설정을 .link.env에 저장 → run_and_record.sh·record_peer.sh·play_bag.sh가 그대로 사용
# NOTE: set -u 사용 금지 — ROS setup.bash가 미설정 변수를 참조해서 죽음.

set -o pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LINK_FILE="$HERE/.link.env"
TEAM_TOPICS=(/target /motor_cmd /tracking_status)    # 팀 공통 인터페이스 (있으면 연동 성공으로 봄)

ok()   { echo "  ✔ $*"; }
warn() { echo "  ⚠ $*"; WARNS=$((WARNS + 1)); }
fail() { echo "  ✘ $*"; FAILS=$((FAILS + 1)); }
step() { echo; echo "[$1] $2"; }
WARNS=0; FAILS=0

# DDS 탐색(SPDP) 멀티캐스트를 받아 도메인별로 노드를 띄운 기기 IP를 출력 (root 불필요)
#   포트 7400 + 250 × domain, 그룹 239.255.0.1 · 인자로 준 IP는 "← 상대" 표시
#   상대가 멀티캐스트를 끈 경우(static peer 전용)엔 여기 안 잡힐 수 있음
dds_who() {
  python3 - "$@" <<'PY'
import collections, select, socket, struct, sys, time
peers = set(sys.argv[1:])
socks = {}
for d in range(102):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, 'SO_REUSEPORT'):
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    try:
        s.bind(('', 7400 + 250 * d))
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                     struct.pack('4s4s', socket.inet_aton('239.255.0.1'), socket.inet_aton('0.0.0.0')))
    except OSError:
        continue
    socks[s] = d
seen = collections.defaultdict(set)
end = time.time() + 10
while socks and time.time() < end:
    for s in select.select(list(socks), [], [], 0.5)[0]:
        data, (ip, _) = s.recvfrom(65535)
        if data[:4] == b'RTPS':
            seen[socks[s]].add(ip)
for d in sorted(seen):
    print(f'domain {d:2}: ' + ', '.join(ip + (' ← 상대' if ip in peers else '') for ip in sorted(seen[d])))
found = set().union(*seen.values()) if seen else set()
if peers and not peers & found:
    print(f'→ 상대({", ".join(sorted(peers))})는 어느 도메인에서도 탐색 신호 없음: 상대 노드가 꺼져 있거나,')
    print('  Docker 브리지 네트워크(network_mode: host 아님)·멀티캐스트 꺼짐 상태일 수 있음')
elif peers & found:
    print('→ 상대가 있는 도메인 번호로 맞추거나 상대를 약속한 도메인으로: ./connect.sh <상대> --domain <번호>')
if not seen:
    print('멀티캐스트 탐색 신호가 하나도 없음 (네트워크가 멀티캐스트를 막는 중일 수 있음)')
PY
}

# zip·Windows를 거치며 생기는 문제 정리: 줄바꿈(CRLF) 제거, 실행 권한 복구
for f in "$HERE"/*.sh "$HERE"/*.py; do
  [ -f "$f" ] || continue
  grep -q $'\r' "$f" 2>/dev/null && sed -i 's/\r$//' "$f"
  [ -x "$f" ] || chmod +x "$f" 2>/dev/null
done

# ---- 인자
# shellcheck disable=SC1090
[ -f "$LINK_FILE" ] && . "$LINK_FILE"
NEW_PEERS=(); DOMAIN=""; MODE=""; ASK=1; BUILD=1; DOCKER=""
while [ $# -gt 0 ]; do
  case "$1" in
    --local)   MODE=local ;;
    --domain)  DOMAIN="$2"; shift ;;
    --ws)      export LV2_WS="$2"; shift ;;
    --ros)     if [ -f "$2" ]; then export ROS_SETUP="$2"; else export ROS_DISTRO_NAME="$2"; fi; shift ;;
    --no-build) BUILD=0 ;;
    --yes|-y)  ASK=0 ;;
    --docker)  DOCKER="$2"; shift ;;
    -h|--help) sed -n '2,23p' "$0"; exit 0 ;;
    -*)        echo "알 수 없는 옵션: $1 (./connect.sh --help)"; exit 1 ;;
    *)         NEW_PEERS+=("$1") ;;
  esac
  shift
done
[ -t 0 ] || ASK=0

# ---- Docker: 이 폴더(압축본·스크립트만)를 컨테이너에 넣고 그 안에서 다시 실행
if [ -n "$DOCKER" ]; then
  command -v docker >/dev/null || { echo "docker 없음"; exit 1; }
  echo "=== $DOCKER 컨테이너의 /tmp/lv2_recordings 로 복사 (bag 원본 폴더는 제외, .tar.gz만)"
  tar -C "$HERE" --exclude='./bags/*/' --exclude='./bags/_empty' --exclude='./.link.env' -cf - . \
    | docker exec -i "$DOCKER" sh -c 'mkdir -p /tmp/lv2_recordings && tar -C /tmp/lv2_recordings -xf -' || exit 1
  ARGS=(); [ -n "$DOMAIN" ] && ARGS+=(--domain "$DOMAIN"); [ "$MODE" = local ] && ARGS+=(--local)
  [ "$BUILD" = 0 ] && ARGS+=(--no-build)
  TTY=(-i); [ -t 0 ] && TTY=(-it)
  exec docker exec "${TTY[@]}" "$DOCKER" bash /tmp/lv2_recordings/connect.sh "${NEW_PEERS[@]}" "${ARGS[@]}"
fi

# ======================================================================
step 1 "기기 정보"
OS="$( (. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME") || uname -s)"
ARCH="$(uname -m)"
PYV="$(python3 -c 'import sys; print(sys.version.split()[0])' 2>/dev/null || echo 없음)"
IN_DOCKER=no; [ -f /.dockerenv ] && IN_DOCKER=yes
ok "OS: $OS · CPU: $ARCH · Python: $PYV · 호스트: $(hostname) · Docker 내부: $IN_DOCKER"
[ "$PYV" = 없음 ] && fail "python3 없음"

# ======================================================================
step 2 "ROS 2 탐지·점검"
# 저장된 배포판이 이 기기에 없으면(다른 기기에서 받은 .link.env) 무시하고 다시 찾는다
[ -n "$LINK_DISTRO" ] && [ ! -f "/opt/ros/$LINK_DISTRO/setup.bash" ] && LINK_DISTRO=""
[ -n "$LINK_SETUP" ] && [ ! -f "$LINK_SETUP" ] && LINK_SETUP=""
export LINK_DISTRO LINK_SETUP
# shellcheck disable=SC1091
if ! source "$HERE/env.sh" --local 2>/dev/null; then
  fail "ROS 2를 찾지 못함"
  if command -v docker >/dev/null && docker ps --format '{{.Names}}' >/dev/null 2>&1; then
    C=$(docker ps --format '{{.Names}}' | while read -r n; do
          docker exec "$n" sh -c 'ls -d /opt/ros/*/setup.bash' >/dev/null 2>&1 && echo "$n"; done)
    [ -n "$C" ] && echo "    ROS가 있는 실행 중인 컨테이너: $(echo $C) → ./connect.sh --docker <이름> ${NEW_PEERS[*]}"
  fi
  echo "    설치: https://docs.ros.org (Ubuntu 24.04 → jazzy: sudo apt install ros-jazzy-ros-base)"
  echo "    소스 빌드한 ROS: ./connect.sh --ros <setup.bash 경로>"
  exit 1
fi
APT_ROS=no
dpkg -l "ros-$ROS_DISTRO-ros2cli" 2>/dev/null | grep -q ^ii && APT_ROS=yes
ok "ROS $ROS_DISTRO ($ROS_PREFIX, $([ "$APT_ROS" = yes ] && echo apt 설치 || echo 소스 빌드)) · RMW: $RMW_IMPLEMENTATION"
OTHERS=$(ls -d /opt/ros/*/ 2>/dev/null | xargs -rn1 basename | grep -vx "$ROS_DISTRO" | tr '\n' ' ')
[ -n "$OTHERS" ] && echo "    (이 기기의 다른 배포판: $OTHERS— 바꾸려면 ./connect.sh --ros <배포판>)"

pkg_hint() {  # 패키지 설치 안내
  if [ "$APT_ROS" = yes ]; then echo "sudo apt install ros-$ROS_DISTRO-${1//_/-}"; else echo "$ROS_DISTRO용 소스 빌드 필요"; fi
}
for p in rclpy std_msgs geometry_msgs sensor_msgs launch launch_ros ros2bag rosbag2_py rosbag2_storage_mcap "$RMW_IMPLEMENTATION"; do
  [ -d "$ROS_PREFIX/share/$p" ] || ros2 pkg prefix "$p" >/dev/null 2>&1 || fail "필수 패키지 없음: $p ($(pkg_hint "$p"))"
done
python3 -c 'import rclpy' 2>/dev/null || fail "rclpy import 실패 — ROS가 다른 Python 버전용으로 빌드됨 (시스템 $PYV)"
for m in cv2:python3-opencv numpy:python3-numpy yaml:python3-yaml; do
  python3 -c "import ${m%%:*}" 2>/dev/null || fail "Python 모듈 없음: ${m%%:*} (sudo apt install ${m#*:})"
done
for t in tar sha256sum; do command -v "$t" >/dev/null || fail "명령 없음: $t"; done
ros2 pkg prefix rosbridge_server >/dev/null 2>&1 \
  && ok "rosbridge_server 있음 (웹 GUI 사용 가능)" \
  || warn "rosbridge_server 없음 → 웹 GUI(index.html) 연결 불가 ($(pkg_hint rosbridge_suite))"
ros2 pkg prefix realsense2_camera >/dev/null 2>&1 \
  && ok "realsense2_camera 있음 (실기 카메라 자동 전환 가능)" \
  || warn "realsense2_camera 없음 → 실기 RealSense 대신 가상 카메라만 ($(pkg_hint realsense2_camera))"
[ "$FAILS" = 0 ] && ok "필수 항목 통과"

# ======================================================================
step 3 "워크스페이스·빌드"
if [ -z "$WS" ]; then
  # 저장소 밖(zip)으로 받은 경우: 홈 아래에서 cognitive_control 워크스페이스를 찾아본다
  CAND=$(find "$HOME" -maxdepth 6 -path '*/src/cognitive_control/package.xml' -not -path '*/install/*' 2>/dev/null | head -1)
  [ -n "$CAND" ] && WS="$(cd "$(dirname "$CAND")/../.." && pwd)"
fi
if [ -n "$WS" ]; then
  ok "워크스페이스: $WS"
  if [ "$BUILD" = 1 ]; then
    if command -v colcon >/dev/null; then
      if lv2_build >"${TMPDIR:-/tmp}/lv2_build.log" 2>&1; then
        ok "빌드 완료 ($ROS_DISTRO) — 실행 파일: $(ros2 pkg executables cognitive_control 2>/dev/null | awk '{print $2}' | tr '\n' ' ')"
      else
        fail "빌드 실패 — 로그: ${TMPDIR:-/tmp}/lv2_build.log"; tail -15 "${TMPDIR:-/tmp}/lv2_build.log" | sed 's/^/      /'
      fi
    else
      fail "colcon 없음 (sudo apt install python3-colcon-common-extensions)"
    fi
  fi
else
  warn "cognitive_control 워크스페이스 없음 → 녹화·재생 전용 (record_peer.sh, play_bag.sh 사용 가능 / run_and_record.sh는 --ws 지정 필요)"
fi

# ======================================================================
step 4 "VS Code 경로"
REPO=""; [ -n "$WS" ] && REPO="$(git -C "$WS" rev-parse --show-toplevel 2>/dev/null)"
if [ -n "$REPO" ] && [ -d "$REPO/.vscode" ]; then
  PYSITE="$(python3 -c 'import os, rclpy; print(os.path.dirname(os.path.dirname(rclpy.__file__)))' 2>/dev/null)"
  ln -sfn "$ROS_PREFIX/include" "$REPO/.vscode/ros_include"
  [ -n "$PYSITE" ] && ln -sfn "$PYSITE" "$REPO/.vscode/ros_python"
  ok ".vscode/ros_include → $ROS_PREFIX/include"
  ok ".vscode/ros_python  → ${PYSITE:-(없음)}"
else
  echo "  - 저장소 밖이라 생략"
fi

# ======================================================================
step 5 "상대 기기 연결 (도메인 자동 탐색)"
[ ${#NEW_PEERS[@]} -gt 0 ] && LINK_PEERS="$(IFS=';'; echo "${NEW_PEERS[*]}")"
if [ "$MODE" != local ] && [ -z "$LINK_PEERS" ] && [ "$ASK" = 1 ]; then
  read -rp "  상대 기기 주소 (예: agumon.local 또는 192.168.0.10, 여러 대는 ;로 구분, 엔터=로컬만): " LINK_PEERS
fi
[ "$MODE" = local ] && LINK_PEERS=""
if [ -n "$DOMAIN" ] && ! { [[ "$DOMAIN" =~ ^[0-9]+$ ]] && [ "$DOMAIN" -le 101 ]; }; then
  fail "--domain은 0~101 (Fast DDS 안전 범위): $DOMAIN"; DOMAIN=""
fi
# env.sh를 다시 source하면 .link.env의 옛 값이 돌아오므로 고른 값을 따로 보관
SEL_PEERS="$LINK_PEERS"; SEL_DOMAIN="$LINK_DOMAIN_ID"; SEL_WS="$WS"

if [ -z "$SEL_PEERS" ]; then
  echo "  - 상대 없음: 로컬 전용 (나중에 ./connect.sh <상대 주소>)"
else
  IFS=';' read -ra PL <<<"$SEL_PEERS"
  PEER_IPS=()
  for h in "${PL[@]}"; do
    ip=$(getent ahostsv4 "$h" 2>/dev/null | awk 'NR==1{print $1}')
    if [ -z "$ip" ]; then fail "$h: 주소를 찾을 수 없음 (.local은 avahi-daemon 필요 → IP로 지정해 보세요)"; continue; fi
    PEER_IPS+=("$ip")
    if ping -c1 -W2 "$ip" >/dev/null 2>&1; then ok "$h ($ip) 응답"; else warn "$h ($ip) ping 응답 없음 (방화벽일 수 있음, 계속 진행)"; fi
  done
  if command -v ufw >/dev/null && ufw status 2>/dev/null | grep -q "Status: active"; then
    warn "ufw 방화벽 켜짐 → DDS(UDP 7400~7600대) 차단 가능: sudo ufw allow from <상대 IP>"
  fi

  # 상대가 노드를 띄운 도메인 찾기 (--domain을 주면 그 값만 확인)
  FOUND=""
  if [ ${#PEER_IPS[@]} -gt 0 ]; then
    if [ -n "$DOMAIN" ]; then FIND_ARGS=(--hint "$DOMAIN" --max -1)       # 지정한 도메인만 확인
    else FIND_ARGS=(${SEL_DOMAIN:+--hint "$SEL_DOMAIN"}); fi             # 지난번 도메인부터, 없으면 0~101
    FOUND=$(python3 "$HERE/find_domain.py" "${PEER_IPS[@]}" "${FIND_ARGS[@]}" 2> >(sed 's/^/  /' >&2))
  fi
  if [ -z "$FOUND" ]; then
    fail "상대(${PEER_IPS[*]:-$SEL_PEERS})가 노드를 띄운 도메인을 찾지 못함 ($([ -n "$DOMAIN" ] && echo "domain $DOMAIN" || echo "0~101 전체") 확인)"
    echo "      → 상대 기기에서 노드(bringup)가 실행 중인지 확인 (ros2 node list)"
    echo "      → Docker면 network_mode: host 필요 (브리지 네트워크면 밖에서 안 보임)"
    echo "      … 참고: 이 Wi-Fi에서 탐색 신호를 보내는 기기"
    dds_who "${PEER_IPS[@]}" | grep '^domain' | sed 's/^/        /'
  else
    SEL_DOMAIN=$(head -1 <<<"$FOUND" | cut -f1)
    ok "상대 도메인: $SEL_DOMAIN (노드: $(head -1 <<<"$FOUND" | cut -f2))"
    [ "$(wc -l <<<"$FOUND")" -gt 1 ] && echo "      (다른 도메인에도 노드 있음: $(tail -n +2 <<<"$FOUND" | cut -f1 | tr '\n' ' ')— 바꾸려면 --domain <번호>)"

    # 찾은 도메인으로 연동 설정 → 상대 토픽 확인 (다른 배포판 daemon과 섞이지 않게 --no-daemon)
    PEERS="$SEL_PEERS" DOMAIN_ID="$SEL_DOMAIN" LINK=1
    export PEERS DOMAIN_ID
    # shellcheck disable=SC1091
    source "$HERE/env.sh" --link
    TOPICS=$(ros2 topic list --no-daemon --spin-time 6 2>/dev/null | grep -vxE '/(rosout|parameter_events)' || true)
    if [ -z "$TOPICS" ]; then
      warn "노드는 있지만 아직 발행 중인 토픽이 없음"
    else
      ok "상대 토픽 $(wc -l <<<"$TOPICS")개:"; sed 's/^/      /' <<<"$TOPICS"
      MISS=(); for t in "${TEAM_TOPICS[@]}"; do grep -qx "$t" <<<"$TOPICS" || MISS+=("$t"); done
      [ ${#MISS[@]} -eq 0 ] && ok "팀 인터페이스(${TEAM_TOPICS[*]}) 확인 — 연동 완료" \
                            || warn "팀 인터페이스 중 안 보이는 토픽: ${MISS[*]} (토픽 이름이 다른지 상대와 확인)"
    fi
  fi
fi

# ======================================================================
step 6 "설정 저장"
cat >"$LINK_FILE" <<EOF
# connect.sh가 $(date '+%F %T') $(hostname)에서 기록 — 이 기기 전용 (git·zip에 포함하지 않음)
LINK_DISTRO=${ROS_DISTRO}
LINK_SETUP=${ROS_SETUP_FILE}
LINK_WS=${SEL_WS}
LINK_PEERS='${SEL_PEERS}'
LINK_DOMAIN_ID=${SEL_DOMAIN}
EOF
ok "$LINK_FILE"

echo
echo "================ 결과: 실패 $FAILS · 경고 $WARNS ================"
cat <<EOF
다음 명령 (recordings 폴더에서)
  ./record_peer.sh            상대 기기 토픽 녹화 → bags/ 압축·README 등록
  ./play_bag.sh               bag 목록 → ./play_bag.sh <이름> 으로 재생 (LINK=1이면 상대 쪽으로, /motor_cmd 제외)
  ./run_and_record.sh         이 PC에서 노드 실행 + 녹화 (워크스페이스 필요, LINK=1이면 상대와 같은 네트워크)
  ./share_zip.sh              팀원에게 보낼 zip 만들기
현재 터미널에서 ros2 명령을 같은 설정으로 쓰려면:
  source "$HERE/env.sh" --link
EOF
[ "$FAILS" = 0 ]
