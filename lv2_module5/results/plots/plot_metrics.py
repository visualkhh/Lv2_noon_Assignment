"""results/metrics.csv → run별 오차·명령 그래프(PNG)와 md용 요약 값.

    python3 plot_metrics.py                 # 같은 폴더에 kp_compare.png 저장, 요약 출력
    python3 plot_metrics.py ../metrics.csv  # CSV 지정

미검출 프레임의 ex는 빈칸 → 선을 끊어서 그린다 (0으로 그리지 않음).
command는 /motor_cmd의 Δpan 명령값이며 실제 모터 위치가 아니다.
"""
import csv
import math
import sys
from collections import OrderedDict
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402

plt.rcParams['font.family'] = 'Noto Sans CJK JP'  # 한글 글리프 포함
plt.rcParams['axes.unicode_minus'] = False

HERE = Path(__file__).resolve().parent
csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent / 'metrics.csv'

runs = OrderedDict()
for r in csv.DictReader(open(csv_path, encoding='utf-8')):
    runs.setdefault(r['run_id'], []).append(r)

fig, axes = plt.subplots(len(runs), 1, figsize=(11, 3.6 * len(runs)), sharex=True, squeeze=False)
for ax, (run_id, rows) in zip(axes[:, 0], runs.items()):
    t = [float(r['time_s']) for r in rows]
    ex = [float(r['ex']) if r['ex'] else math.nan for r in rows]
    cmd = [float(r['command']) for r in rows]
    tracking = [r['state'] == 'TRACKING' for r in rows]

    # TRACKING 구간 음영
    start = None
    for i, on in enumerate(tracking + [False]):
        if on and start is None:
            start = t[i]
        if not on and start is not None:
            ax.axvspan(start, t[i - 1], color='tab:green', alpha=0.08, lw=0)
            start = None

    ax.axhspan(-0.05, 0.05, color='gray', alpha=0.15, lw=0, label='데드밴드 ±0.05')
    ax.axhline(0, color='gray', lw=0.6)
    ax.plot(t, ex, color='tab:blue', lw=1.2, label='ex (정규화 수평 오차)')
    ax.set_ylim(-1.05, 1.05)
    ax.set_ylabel('ex')

    ax2 = ax.twinx()
    ax2.step(t, cmd, where='post', color='tab:orange', lw=1.0, alpha=0.8, label='명령 Δpan [rad/프레임]')
    lim = max(0.02, max(abs(c) for c in cmd) * 1.2)
    ax2.set_ylim(-lim, lim)
    ax2.set_ylabel('Δpan 명령 [rad]')

    gains = [float(r['command']) / float(r['ex']) for r in rows
             if r['ex'] and float(r['command']) != 0 and abs(float(r['ex'])) > 0.05]
    kp = f'{sorted(gains)[len(gains) // 2]:.3f}' if gains else '?'
    ax.set_title(f'{run_id}  (Kp = {kp}, 초록 음영 = TRACKING)', loc='left', fontsize=10)

    h1, l1 = ax.get_legend_handles_labels()
    h2, l2 = ax2.get_legend_handles_labels()
    ax.legend(h1 + h2, l1 + l2, loc='upper right', fontsize=8)

axes[-1, 0].set_xlabel('시간 [s]')
fig.tight_layout()
out = HERE / 'kp_compare.png'
fig.savefig(out, dpi=120)
print(f'저장: {out}')

# md(mermaid)용 요약
for run_id, rows in runs.items():
    n = len(rows)
    det = [r for r in rows if r['detected'] == '1']
    trk = [float(r['ex']) for r in det if r['state'] == 'TRACKING']
    dur = float(rows[-1]['time_s'])
    per_s = []
    for s in range(int(dur) + 1):
        b = [r for r in rows if s <= float(r['time_s']) < s + 1]
        per_s.append(round(100 * sum(r['detected'] == '1' for r in b) / len(b)) if b else 0)
    print(f'{run_id}: fps={(n - 1) / dur:.2f} 검출률={100 * len(det) / n:.0f}% '
          f'TRACKING={100 * sum(r["state"] == "TRACKING" for r in rows) / n:.0f}% '
          f'RMSE={math.sqrt(sum(e * e for e in trk) / len(trk)):.3f} '
          f'명령프레임={sum(float(r["command"]) != 0 for r in rows)}')
    print(f'  초별 검출률: {per_s}')
