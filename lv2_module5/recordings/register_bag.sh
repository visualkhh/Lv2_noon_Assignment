#!/usr/bin/env bash
# 사용법: ./register_bag.sh <bag 이름> [장면 설명]
#   bags/<이름>/metadata.yaml을 읽어 README.md [목록]에 [목록:<이름>.tar.gz] 항목을 등록 (같은 이름이면 교체)
#   - 녹화 일시·기간·토픽(메시지 수)은 metadata.yaml에서 → ROS 배포판과 무관하게 동작
#   - 장면 설명을 안 주면 기존 항목의 설명을 유지, 없으면 터미널에서 입력받음
#   - /camera_source·/opencr_status가 있으면 카메라·OpenCR 상태를 장면 칸에 덧붙임 (rosbag2_py 필요, 없으면 생략)
#   - 기준 커밋: 녹화 시각 직전의 커밋 (git 저장소 밖이면 "-")
#   - 실제 데이터 토픽이 없는 bag은 등록하지 않음 (FORCE=1이면 등록)
set -eo pipefail
REC_DIR="$(cd "$(dirname "$0")" && pwd)"
NAME="${1%/}"; NAME="${NAME#bags/}"
[ -n "$NAME" ] || { echo "사용법: $0 <bag 이름> [장면 설명]"; exit 1; }
BAG="$REC_DIR/bags/$NAME"
TGZ="$REC_DIR/bags/$NAME.tar.gz"
[ -f "$BAG/metadata.yaml" ] || { echo "bag 없음: bags/$NAME"; exit 1; }
[ -f "$TGZ" ] || { echo "압축본 없음: bags/$NAME.tar.gz — ./pack_bag.sh $NAME 먼저"; exit 1; }
# 카메라·OpenCR 상태 읽기용 (ROS가 없으면 그 부분만 생략)
python3 -c 'import rosbag2_py' 2>/dev/null || . "$REC_DIR/env.sh" --local >/dev/null 2>&1 || true

# 기간·토픽·시작 시각
read -r DUR START START_ISO TOPIC_COL < <(python3 - "$BAG/metadata.yaml" <<'PY'
import sys, datetime, yaml
i = yaml.safe_load(open(sys.argv[1]))['rosbag2_bagfile_information']
skip = {'/rosout', '/parameter_events', '/events/write_split'}
topics = sorted((t['topic_metadata']['name'], t['message_count'])
                for t in i.get('topics_with_message_count', []))
col = ', '.join(f'{n}({c})' for n, c in topics if n not in skip and c > 0)
ns = i['starting_time']['nanoseconds_since_epoch']
empty = i['message_count'] == 0 or ns >= 2**63 - 1
t = datetime.datetime.fromtimestamp(ns / 1e9)
print(f"{i['duration']['nanoseconds'] / 1e9:.1f}s",
      '-' if empty else t.strftime('%Y-%m-%d_%H:%M:%S'),
      '-' if empty else t.isoformat(timespec='seconds'),
      col.replace(' ', '\x01') or '-')
PY
)
START="${START//_/ }"
TOPIC_COL="${TOPIC_COL//$'\x01'/ }"

if [ "$TOPIC_COL" = "-" ]; then
  echo "⚠ bags/$NAME: 실제 데이터 토픽이 없는 bag"
  [ "${FORCE:-0}" = 1 ] || { echo "README 등록 생략 (등록하려면 FORCE=1)"; exit 0; }
  TOPIC_COL="(데이터 없음)"
fi

# 장면 설명: 인자 > 기존 항목 > 입력
DESC="$2"
if [ -z "$DESC" ]; then
  DESC=$(python3 - "$REC_DIR/README.md" "$NAME.tar.gz" <<'PY'
import sys
lines = open(sys.argv[1], encoding='utf-8').read().split('\n')
head = f'[목록:{sys.argv[2]}]'
if head in lines:
    for l in lines[lines.index(head) + 1:]:
        if not l.startswith('- '):
            break
        if l.startswith('- 장면: '):
            d = l[len('- 장면: '):]
            # 자동으로 덧붙인 (카메라: ..., OpenCR: ...) 부분은 다시 계산하므로 제거
            if '(카메라:' in d:
                d = d[:d.index('(카메라:')].rstrip()
            print('' if d == '-' else d)
PY
)
fi
if [ -z "$DESC" ] && [ -t 0 ]; then
  read -rp "bags/$NAME 장면 설명 (README '장면' 칸, 엔터=생략): " DESC
fi
SOURCES=$(python3 - "$BAG" <<'PY' 2>/dev/null || true
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

SIZE=$(du -h "$TGZ" | cut -f1)
SHA=$(sha256sum "$TGZ" | cut -d" " -f1)
COMMIT="-"
if git -C "$REC_DIR" rev-parse --git-dir >/dev/null 2>&1; then
  if [ "$START_ISO" != "-" ]; then
    COMMIT=$(git -C "$REC_DIR" rev-list -1 --before="$START_ISO" HEAD 2>/dev/null | cut -c1-7)
  fi
  COMMIT="${COMMIT:-$(git -C "$REC_DIR" rev-parse --short HEAD)}"
fi

ENTRY="[목록:$NAME.tar.gz]
- 녹화 일시: $START
- 장면: $DESC
- 기간: $DUR
- 토픽: $TOPIC_COL
- 용량: $SIZE
- SHA256: \`$SHA\`
- 기준 커밋: \`$COMMIT\`
- 다운로드: (업로드 후 기입)"

# 같은 파일 항목은 교체(다운로드 링크는 유지), 없으면 [목록] 끝에 녹화 일시 순으로 추가
python3 - "$REC_DIR/README.md" "$NAME.tar.gz" "$ENTRY" <<'PY'
import sys
path, f, entry = sys.argv[1:]
lines = open(path, encoding='utf-8').read().rstrip('\n').split('\n')
new = entry.split('\n')
head = f'[목록:{f}]'
if head in lines:
    a = lines.index(head)
    b = a + 1
    while b < len(lines) and lines[b].startswith('- '):
        if lines[b].startswith('- 다운로드: ') and '(업로드 후 기입)' not in lines[b]:
            new[-1] = lines[b]
        b += 1
    lines[a:b] = new
else:
    if '[목록]' not in lines:
        lines += ['', '[목록]']
    lines += [''] + new
open(path, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
PY
echo "---- README.md 등록 ----"
echo "$ENTRY"
