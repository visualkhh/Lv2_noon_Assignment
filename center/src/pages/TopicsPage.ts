import { createElement, type CreateElementConfig, elementDefine, onConnectedBodyShadow, onConnectedAfter, onInitialize, onDisconnected, eventDelegateShadow, matchedElement, eventShadow, eventObject, subscribeSwcAppMessage, appMessage } from '@dooboostore/simple-web-component';
import type { SwcAppMessage } from '@dooboostore/simple-web-component';
import { inject } from '@dooboostore/simple-boot';
import { ControlService } from '@app-src/services/ControlService';
import { TARGET_CHANGED, esc } from '@app-src/utils/ui';

export const tagName = 'topics-page';

export interface TopicsPage extends HTMLElement {}

export const TopicsPage = (w: Window, data?: CreateElementConfig) => {
  return createElement<TopicsPage>(w, tagName, data);
};

const STALE_S = 2.0;

export const defineTopicsPage = async (w: Window) => {
  const existing = w.customElements.get(tagName);
  if (existing) return existing;

  @elementDefine(tagName, { window: w })
  class TopicsPageImp extends (w.HTMLElement as unknown as typeof HTMLElement) implements TopicsPage {
    private controlService?: ControlService;
    private timer?: ReturnType<typeof setInterval>;
    private target = '';
    private topics: ControlService.StatusItem[] = [];
    private imageNames = new Set<string>();
    private selected = '';
    private filter = '';

    @onInitialize
    init(@inject(ControlService.SYMBOL) controlService: ControlService) {
      this.controlService = controlService;
    }

    @subscribeSwcAppMessage(TARGET_CHANGED, { subject: 'behavior' })
    onTarget(@appMessage msg: SwcAppMessage<string>) {
      if (msg?.data && msg.data !== this.target) {
        this.target = msg.data;
        this.selected = '';
        void this.refresh();
      }
    }

    @onConnectedAfter
    async start() {
      try {
        const { targets } = await this.controlService!.listTargets({});
        if (targets.length && !this.target) this.target = targets[0].id;
      } catch {
        /* 무시 */
      }
      await this.refresh();
      this.timer = setInterval(() => void this.refresh(), 1500);
    }

    @onDisconnected
    stop() {
      if (this.timer) clearInterval(this.timer);
      this.timer = undefined;
    }

    @eventShadow('.q', 'input')
    onSearch(@eventObject e: Event) {
      this.filter = (e.target as HTMLInputElement).value.toLowerCase();
      this.renderList();
    }

    @eventDelegateShadow('.topic', 'click')
    onSelect(@matchedElement el: Element) {
      this.selected = el.getAttribute('data-name') ?? '';
      this.renderList();
      void this.refreshDetail();
    }

    private filtered() {
      return this.topics.filter((t) => !this.filter || t.name.toLowerCase().includes(this.filter));
    }

    private async refresh() {
      if (!this.controlService || !this.target) return;
      try {
        const [status, images] = await Promise.all([
          this.controlService.getStatus({ target: this.target }),
          this.controlService.getImages({ target: this.target })
        ]);
        this.topics = status.items;
        this.imageNames = new Set(images.map((im) => im.name));
        if (!this.topics.some((t) => t.name === this.selected)) {
          this.selected = this.filtered()[0]?.name ?? '';
        }
        this.renderList();
        await this.refreshDetail();
      } catch {
        /* 무시 */
      }
    }

    private renderList() {
      const box = this.shadowRoot?.querySelector('.list');
      if (!box) return;
      const st = (box as HTMLElement).scrollTop;
      const items = this.filtered();
      box.innerHTML = items.length
        ? items
            .map((t) => {
              const stale = t.ageS == null || t.ageS > STALE_S;
              const age = t.ageS == null ? '—' : `${t.ageS.toFixed(1)}s`;
              const hasImg = this.imageNames.has(t.name) ? '<span class="tag">img</span>' : '';
              return `<button class="topic${t.name === this.selected ? ' sel' : ''}${stale ? ' stale' : ''}" data-name="${esc(t.name)}"><span class="dot"></span><span class="nm">${esc(t.name)}</span>${hasImg}<span class="age">${esc(age)}</span></button>`;
            })
            .join('')
        : '<p class="empty">토픽 없음</p>';
      (box as HTMLElement).scrollTop = st;
      const count = this.shadowRoot?.querySelector('.count');
      if (count) count.textContent = `${items.length}/${this.topics.length}`;
    }

    private async refreshDetail() {
      const detail = this.shadowRoot?.querySelector('.detail');
      if (!detail || !this.controlService || !this.target || !this.selected) {
        if (detail && !this.selected) detail.innerHTML = '<p class="empty">왼쪽에서 토픽을 고르세요</p>';
        return;
      }
      const t = this.topics.find((x) => x.name === this.selected);
      const stale = !t || t.ageS == null || t.ageS > STALE_S;
      const age = !t || t.ageS == null ? '—' : `${t.ageS.toFixed(1)}s`;
      const hasImg = this.imageNames.has(this.selected);
      const key = `${this.selected}|${hasImg}`;
      let wrap = detail.querySelector('.d-wrap') as HTMLElement | null;
      if (!wrap || wrap.dataset.key !== key) {
        detail.innerHTML = `<div class="d-wrap" data-key="${esc(key)}">
          <div class="d-head"><h2>${esc(this.selected)}</h2>
            <span class="badge ${stale ? 'stale' : 'fresh'}">${stale ? 'STALE' : 'LIVE'} · ${esc(age)}</span>
            ${t?.type ? `<span class="badge type">${esc(t.type)}</span>` : ''}</div>
          ${hasImg ? `<img class="d-img" src="/api/center/stream?target=${encodeURIComponent(this.target)}&name=${encodeURIComponent(this.selected)}" alt="" />` : ''}
          <h3>message</h3><pre class="msg"></pre>
          <h3>echo <span class="echo-state"></span></h3><pre class="echo"></pre>
        </div>`;
        wrap = detail.querySelector('.d-wrap');
      } else {
        const badge = wrap.querySelector('.badge');
        if (badge) {
          badge.className = `badge ${stale ? 'stale' : 'fresh'}`;
          badge.textContent = `${stale ? 'STALE' : 'LIVE'} · ${age}`;
        }
      }
      const msg = wrap?.querySelector('.msg');
      if (msg) msg.textContent = t ? JSON.stringify(t.data, null, 1) : '(없음)';
      try {
        const echo = await this.controlService.getEcho({ target: this.target, name: this.selected });
        const box = wrap?.querySelector('.echo');
        const state = wrap?.querySelector('.echo-state');
        if (box) box.textContent = echo.exists ? (echo.lines.join('\n') || '(빈 echo)') : '(echo 없음 — test-logger 실행 확인)';
        if (state) state.textContent = echo.exists ? '' : '';
      } catch {
        /* 무시 */
      }
    }

    @onConnectedBodyShadow
    render() {
      return `
      <style>
        :host { display: block; padding: 16px; }
        .toolbar { display: flex; gap: 8px; align-items: center; margin-bottom: 12px; }
        .toolbar input { font: inherit; font-size: 13px; color: var(--text); background: var(--surface); border: 1px solid var(--border); padding: 8px 12px; border-radius: 8px; width: 280px; }
        .count { color: var(--muted); font-size: 13px; font-family: var(--font-mono); }
        .cols { display: grid; grid-template-columns: 360px 1fr; gap: 12px; align-items: start; }
        @media (max-width: 1000px) { .cols { grid-template-columns: 1fr; } }
        .list { display: flex; flex-direction: column; gap: 4px; max-height: calc(100vh - 180px); overflow: auto; }
        .topic { display: flex; gap: 8px; align-items: center; text-align: left; font: inherit; font-size: 13px; color: var(--text); padding: 8px 10px; border-radius: 8px; border: 1px solid var(--border); background: var(--surface); cursor: pointer; }
        .topic:hover { background: var(--surface2); }
        .topic.sel { border-color: var(--brand); background: var(--brand-dim); }
        .topic .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--green); flex: none; }
        .topic.stale .dot { background: var(--red); }
        .topic .nm { flex: 1; word-break: break-all; font-family: var(--font-mono); font-size: 12px; }
        .topic .age { color: var(--green); font-size: 12px; font-family: var(--font-mono); flex: none; }
        .topic.stale .age { color: var(--red); }
        .tag { font-size: 10px; background: var(--green-dim); color: var(--green); border-radius: 4px; padding: 1px 5px; flex: none; }
        .detail { border: 1px solid var(--border); border-radius: var(--radius); background: var(--surface); padding: 14px; min-height: 300px; }
        .d-head { display: flex; gap: 8px; align-items: center; margin-bottom: 10px; flex-wrap: wrap; }
        .d-head h2 { margin: 0; font-size: 15px; font-family: var(--font-mono); }
        .badge { font-size: 11px; font-weight: 700; border-radius: 999px; padding: 2px 10px; }
        .badge.fresh { background: var(--green-dim); color: var(--green); }
        .badge.stale { background: var(--red-dim); color: var(--red); }
        .badge.type { background: var(--brand-dim); color: var(--brand); }
        .d-img { width: 100%; max-width: 560px; border-radius: 8px; border: 1px solid var(--border); background: #000; margin-bottom: 10px; }
        h3 { margin: 12px 0 6px; font-size: 12px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.05em; }
        pre { background: #0b0b12; color: #c9e7c9; font-size: 12px; font-family: var(--font-mono); padding: 10px 12px; border-radius: 8px; overflow: auto; max-height: 320px; margin: 0; white-space: pre-wrap; word-break: break-all; }
        pre.echo { color: #9fd0ff; max-height: 200px; }
        .empty { color: var(--muted); font-size: 13px; }
      </style>
      <div class="toolbar"><input class="q" placeholder="토픽 검색 (예: /target)" /><span class="count"></span></div>
      <div class="cols"><div class="list"></div><div class="detail"><p class="empty">불러오는 중…</p></div></div>`;
    }
  }

  return w.customElements.whenDefined(tagName);
};

export default defineTopicsPage;
