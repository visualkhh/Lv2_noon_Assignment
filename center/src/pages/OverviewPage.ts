import { createElement, type CreateElementConfig, elementDefine, onConnectedBodyShadow, onConnectedAfter, onInitialize, onDisconnected, eventDelegateShadow, matchedElement, eventShadow, eventObject, publishSwcAppMessage, subscribeSwcAppMessage, appMessage } from '@dooboostore/simple-web-component';
import type { SwcAppMessage } from '@dooboostore/simple-web-component';
import { inject } from '@dooboostore/simple-boot';
import { ControlService } from '@app-src/services/ControlService';
import { TARGET_CHANGED, esc } from '@app-src/utils/ui';

export const tagName = 'overview-page';

export interface OverviewPage extends HTMLElement {}

export const OverviewPage = (w: Window, data?: CreateElementConfig) => {
  return createElement<OverviewPage>(w, tagName, data);
};

const STALE_S = 2.0;

const dial = (id: string, label: string) => `
  <div class="dial">
    <svg viewBox="0 0 120 132">
      <circle cx="60" cy="60" r="52" class="ring" />
      ${Array.from({ length: 8 }, (_, i) => {
        const a = (i * 45 * Math.PI) / 180;
        const x1 = 60 + 52 * Math.sin(a);
        const y1 = 60 - 52 * Math.cos(a);
        const x2 = 60 + 44 * Math.sin(a);
        const y2 = 60 - 44 * Math.cos(a);
        return `<line x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}" class="${i === 0 ? 'tick0' : 'tick'}" />`;
      }).join('')}
      <line x1="60" y1="60" x2="60" y2="14" class="needle ${id}" />
      <circle cx="60" cy="60" r="5" class="hub" />
      <text x="60" y="126" text-anchor="middle" class="val ${id}-val">—</text>
    </svg>
    <div class="dial-label">${esc(label)}</div>
  </div>`;

export const defineOverviewPage = async (w: Window) => {
  const existing = w.customElements.get(tagName);
  if (existing) return existing;

  @elementDefine(tagName, { window: w })
  class OverviewPageImp extends (w.HTMLElement as unknown as typeof HTMLElement) implements OverviewPage {
    private controlService?: ControlService;
    private timer?: ReturnType<typeof setInterval>;
    private target = '';
    private streaming = false;

    @onInitialize
    init(@inject(ControlService.SYMBOL) controlService: ControlService) {
      this.controlService = controlService;
    }

    @publishSwcAppMessage(TARGET_CHANGED)
    publishTarget(target: string) {
      return target;
    }

    @subscribeSwcAppMessage(TARGET_CHANGED, { subject: 'behavior' })
    onTarget(@appMessage msg: SwcAppMessage<string>) {
      if (msg?.data && msg.data !== this.target) {
        this.target = msg.data;
        void this.refresh();
      }
    }

    @onConnectedAfter
    async start() {
      try {
        const { targets } = await this.controlService!.listTargets({});
        if (targets.length && !this.target) {
          this.target = targets[0].id;
          this.publishTarget(this.target);
        }
        this.renderTargets(targets);
      } catch {
        /* 백엔드 미연결 — 빈 화면 */
      }
      await this.refresh();
      this.timer = setInterval(() => void this.refresh(), 1000);
    }

    @onDisconnected
    stop() {
      if (this.timer) clearInterval(this.timer);
      this.timer = undefined;
    }

    private renderTargets(targets: ControlService.Target[]) {
      const sel = this.shadowRoot?.querySelector('.targets') as HTMLSelectElement | null;
      if (!sel) return;
      sel.innerHTML = targets.map((t) => `<option value="${esc(t.id)}" ${t.id === this.target ? 'selected' : ''}>${esc(t.id)} — ${esc(t.path)}</option>`).join('') || '<option value="">(debug 폴더 없음)</option>';
    }

    @eventShadow('.targets', 'change')
    onTargetChange(@eventObject e: Event) {
      this.target = (e.target as HTMLSelectElement).value;
      this.publishTarget(this.target);
      void this.refresh();
    }

    private async refresh() {
      if (!this.controlService || !this.target) return;
      const svc = this.controlService;
      const t = this.target;
      try {
        const [status, motors, images, sout, sin] = await Promise.all([
          svc.getStatus({ target: t }),
          svc.getMotors({ target: t }),
          svc.getImages({ target: t }),
          svc.getSerial({ target: t, kind: 'out' }),
          svc.getSerial({ target: t, kind: 'in' })
        ]);
        this.renderState(status.items);
        this.renderMotors(motors);
        this.renderImages(images);
        this.renderSerial('.serial-out', sout.lines);
        this.renderSerial('.serial-in', sin.lines);
      } catch {
        /* 폴링 실패 무시 */
      }
    }

    private renderState(items: ControlService.StatusItem[]) {
      const pill = this.shadowRoot?.querySelector('.state-pill');
      const sub = this.shadowRoot?.querySelector('.state-sub');
      const st = items.find((i) => i.name === '/tracking_status');
      const tgt = items.find((i) => i.name === '/target');
      const state = typeof st?.data === 'string' ? st.data : (st?.data as any)?.data ?? '—';
      const fresh = st && st.ageS != null && st.ageS <= STALE_S;
      if (pill) {
        pill.className = `state-pill ${state === 'TRACKING' && fresh ? 'tracking' : state === 'LOST' ? 'lost' : 'idle'}`;
        pill.textContent = `● ${state}`;
      }
      if (sub) {
        const ex = (tgt?.data as any)?.point;
        sub.textContent =
          tgt && tgt.ageS != null
            ? `/target ${tgt.ageS.toFixed(1)}s 전` + (ex && typeof ex.x === 'number' ? ` · ex ${ex.x >= 0 ? '+' : ''}${ex.x.toFixed(3)} ey ${ex.y >= 0 ? '+' : ''}${ex.y.toFixed(3)}` : '')
            : 'target 없음';
      }
      const n = this.shadowRoot?.querySelector('.topic-count');
      if (n) n.textContent = `토픽 ${items.length}개 →`;
    }

    private setDial(id: string, angle: number) {
      const needle = this.shadowRoot?.querySelector(`.needle.${id}`) as SVGLineElement | null;
      const val = this.shadowRoot?.querySelector(`.${id}-val`);
      if (needle) needle.setAttribute('transform', `rotate(${angle.toFixed(2)} 60 60)`);
      if (val) val.textContent = `${angle >= 0 ? '+' : ''}${angle.toFixed(2)}°`;
    }

    private renderMotors(m: ControlService.MotorsResponse) {
      const s = m.serial.count > 0 ? m.serial : m.joint;
      this.setDial('pan', s.pan);
      this.setDial('tilt', s.tilt);
      const info = this.shadowRoot?.querySelector('.motor-info');
      if (info) {
        info.textContent = `serial ${m.serial.count}개 · joint ${m.joint.count}개 · 출처 ${m.source} (추정치 — 실측 위치 아님)`;
      }
    }

    private renderImages(images: ControlService.ImageItem[]) {
      // MJPEG 스트림은 img 태그가 알아서 갱신 — 목록(이름 집합)이 바뀔 때만 다시 그림.
      const box = this.shadowRoot?.querySelector('.images');
      if (!box) return;
      const key = images.map((im) => im.name).join('|');
      if ((box as HTMLElement).dataset.key === key) return;
      (box as HTMLElement).dataset.key = key;
      const shown = images.slice(0, 4);
      box.innerHTML = shown.length
        ? shown
            .map(
              (im) =>
                `<figure><figcaption>${esc(im.name)}</figcaption>` +
                `<img loading="lazy" src="/api/center/stream?target=${encodeURIComponent(this.target)}&name=${encodeURIComponent(im.name)}" alt="${esc(im.name)}" /></figure>`
            )
            .join('')
        : '<p class="empty">이미지 없음 (fake_camera_bringup 실행 확인)</p>';
    }

    private renderSerial(sel: string, lines: string[]) {
      const box = this.shadowRoot?.querySelector(sel);
      if (!box) return;
      box.textContent = lines.join('\n');
      (box as HTMLElement).scrollTop = (box as HTMLElement).scrollHeight;
    }

    @eventDelegateShadow('.stream-toggle', 'click')
    async onStreamToggle() {
      if (!this.controlService || !this.target) return;
      this.streaming = !this.streaming;
      if (!this.streaming) {
        await this.controlService.setStreaming({ target: this.target, on: false });
      }
      // 켜기는 World 페이지에서 three.js 렌더가 putFrame으로 송출
      const btn = this.shadowRoot?.querySelector('.stream-toggle');
      if (btn) {
        btn.textContent = this.streaming ? '● 송출 모드' : '■ 송출';
        btn.classList.toggle('on', this.streaming);
      }
      const label = this.shadowRoot?.querySelector('.stream-state');
      if (label) label.textContent = this.streaming ? 'World 페이지 렌더가 frame.png로 송출' : '꺼짐 (카메라 없음)';
    }

    @eventDelegateShadow('.zero-motors', 'click')
    async onZeroMotors() {
      if (!this.controlService || !this.target) return;
      await this.controlService.resetMotors({ target: this.target });
      void this.refresh();
    }

    @eventDelegateShadow('.serial-send', 'click')
    async onSerialSend() {
      const input = this.shadowRoot?.querySelector('.serial-line') as HTMLInputElement | null;
      const msg = this.shadowRoot?.querySelector('.serial-msg');
      if (!input || !this.controlService || !this.target) return;
      const line = input.value.trim();
      if (!line) return;
      const res = await this.controlService.appendSerial({ target: this.target, line });
      if (msg) msg.textContent = res.ok ? `보냄 ${line}` : `실패: ${res.error}`;
      if (res.ok) input.value = '';
    }

    @onConnectedBodyShadow
    render() {
      return `
      <style>
        :host { display: block; padding: 16px; }
        .toolbar { display: flex; gap: 10px; align-items: center; margin-bottom: 14px; flex-wrap: wrap; }
        .toolbar select { font: inherit; font-size: 13px; color: var(--text); background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 8px 10px; max-width: 420px; }
        .state-pill { font-size: 13px; font-weight: 800; border-radius: 999px; padding: 6px 14px; letter-spacing: 0.02em; }
        .state-pill.tracking { background: var(--green-dim); color: var(--green); }
        .state-pill.lost { background: var(--red-dim); color: var(--red); }
        .state-pill.idle { background: var(--surface2); color: var(--muted); }
        .state-sub { font-size: 12px; color: var(--muted); font-family: var(--font-mono); }
        .spacer { flex: 1; }
        button { font: inherit; font-size: 13px; font-weight: 600; color: var(--text); background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 8px 14px; cursor: pointer; }
        button:hover { background: var(--surface2); }
        button.on { background: var(--green-dim); border-color: var(--green); color: var(--green); }
        button.ghost { background: none; }
        .topic-count { font-size: 12px; color: var(--muted); cursor: pointer; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
        @media (max-width: 1000px) { .grid { grid-template-columns: 1fr; } }
        section { border: 1px solid var(--border); border-radius: var(--radius); padding: 14px; background: var(--surface); }
        section h2 { margin: 0 0 10px; font-size: 13px; font-weight: 700; color: var(--muted); text-transform: uppercase; letter-spacing: 0.06em; }
        .dials { display: flex; gap: 8px; justify-content: space-around; }
        .dial { text-align: center; }
        .dial svg { width: 150px; }
        .dial .ring { fill: none; stroke: var(--border); stroke-width: 2; }
        .dial .tick { stroke: #4a4a66; stroke-width: 1.5; }
        .dial .tick0 { stroke: var(--text); stroke-width: 2; }
        .dial .needle { stroke: var(--brand); stroke-width: 3.5; stroke-linecap: round; }
        .dial .hub { fill: var(--brand); }
        .dial .val { fill: var(--text); font-size: 13px; font-weight: 700; font-family: var(--font-mono); }
        .dial-label { font-size: 12px; color: var(--muted); margin-top: 2px; }
        .motor-info { margin-top: 8px; font-size: 11px; color: var(--muted); font-family: var(--font-mono); }
        .images { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        figure { margin: 0; }
        figcaption { font-size: 11px; color: var(--muted); font-family: var(--font-mono); margin-bottom: 4px; word-break: break-all; }
        .images img { width: 100%; border: 1px solid var(--border); border-radius: 8px; background: #000; aspect-ratio: 4/3; object-fit: contain; }
        .serial-cols { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        .serial-box h3 { margin: 0 0 6px; font-size: 12px; color: var(--muted); font-weight: 600; }
        pre.serial { background: #0b0b12; color: #7de79d; font-size: 11px; font-family: var(--font-mono); height: 150px; overflow: auto; padding: 8px 10px; border-radius: 8px; margin: 0; white-space: pre-wrap; word-break: break-all; }
        .serial-send { display: flex; gap: 6px; margin-top: 8px; align-items: center; }
        .serial-send input { flex: 1; font-family: var(--font-mono); font-size: 12px; color: var(--text); background: var(--surface2); border: 1px solid var(--border); border-radius: 8px; padding: 8px 10px; }
        .serial-msg { font-size: 12px; color: var(--muted); }
        .empty { color: var(--muted); font-size: 13px; }
      </style>
      <div class="toolbar">
        <select class="targets"><option>…</option></select>
        <span class="state-pill idle">● —</span>
        <span class="state-sub"></span>
        <span class="spacer"></span>
        <button class="stream-toggle">■ 송출</button>
        <span class="state-sub stream-state">꺼짐 (카메라 없음)</span>
        <button class="ghost" data-href="/world">3D World →</button>
      </div>
      <div class="grid">
        <section><h2>Motors · 명령 누적</h2>
          <div class="dials">${dial('pan', 'pan')}${dial('tilt', 'tilt')}</div>
          <div class="motor-info"></div>
          <div style="margin-top:8px"><button class="zero-motors">0으로 맞추기 (기구 정면)</button></div>
        </section>
        <section><h2>Images · MJPEG</h2><div class="images"></div></section>
        <section style="grid-column: 1 / -1"><h2>Serial <span class="topic-count" data-href="/topics"></span></h2>
          <div class="serial-cols">
            <div class="serial-box"><h3>serial-out · 앱 → 파일</h3><pre class="serial serial-out"></pre></div>
            <div class="serial-box"><h3>serial-in · 파일 → 앱</h3><pre class="serial serial-in"></pre></div>
          </div>
          <div class="serial-send"><input class="serial-line" value="S,0.0,0.0,0.0,0.0" /><button class="serial-send">보내기</button><span class="serial-msg">Enter 아님 — 버튼 클릭</span></div>
        </section>
      </div>`;
    }
  }

  return w.customElements.whenDefined(tagName);
};

export default defineOverviewPage;
