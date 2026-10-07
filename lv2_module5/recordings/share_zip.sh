#!/usr/bin/env bash
# 사용법: ./share_zip.sh [출력 zip 경로]
#   팀원에게 보낼 recordings zip을 만든다 (기본: recordings 옆에 recordings_share_YYYYmmdd_HHMM.zip)
#   - 포함: 스크립트, README.md, SHA256SUMS, .gitignore, bags/*.tar.gz (등록된 bag 압축본)
#   - 제외: bag 원본 폴더(받는 쪽에서 play_bag.sh가 .tar.gz를 풀어 씀), bags/_empty, .link.env(기기 전용 설정)
#   - NO_BAGS=1 이면 스크립트·문서만 (가볍게)
#   - 실행 권한을 zip 안에 보존 → 받는 쪽: unzip 후 cd recordings && ./connect.sh
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$(dirname "$HERE")/recordings_share_$(date +%Y%m%d_%H%M).zip}"

(cd "$HERE" && sha256sum -c --quiet SHA256SUMS) || { echo "SHA256SUMS 검사 실패 — 손상된 파일 확인 후 다시 실행"; exit 1; }

python3 - "$HERE" "$OUT" "${NO_BAGS:-0}" <<'PY'
import os, sys, zipfile
src, out, no_bags = sys.argv[1], sys.argv[2], sys.argv[3] == '1'
files = sorted(f for f in os.listdir(src)
               if os.path.isfile(os.path.join(src, f)) and f != '.link.env' and not f.endswith('.tmp'))
if not no_bags and os.path.isdir(os.path.join(src, 'bags')):
    files += sorted('bags/' + f for f in os.listdir(os.path.join(src, 'bags')) if f.endswith('.tar.gz'))
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in files:
        z.write(os.path.join(src, f), 'recordings/' + f)   # 권한(실행 비트)도 함께 저장됨
print(f'{out}  ({len(files)}개 파일, {os.path.getsize(out) / 1e6:.1f} MB)')
PY
echo "받는 쪽: unzip $(basename "$OUT") && cd recordings && ./connect.sh <상대 주소>   (권한이 없으면 bash connect.sh)"
