import { elementDefine, onConnectedBodyShadow, subscribeSwcAppRouteChange, replaceChildrenLight, htmlFragment, eventDelegateAll, matchedElement, swcAppRouteGo, attribute, removeAttribute } from '@dooboostore/simple-web-component';
import type { RouterEventType } from '@dooboostore/core-web';
import { OverviewPage } from './OverviewPage';
import { TopicsPage } from './TopicsPage';
import { WorldPage } from './WorldPage';

export const tagName = 'center-router';

export const defineCenterRouter = async (w: Window) => {
  const existing = w.customElements.get(tagName);
  if (existing) return existing;

  @elementDefine(tagName, { window: w })
  class CenterRouterImp extends (w.HTMLElement as unknown as typeof HTMLElement) {
    @replaceChildrenLight({ valueKey: 'element' })
    @subscribeSwcAppRouteChange(['', '/'])
    @attribute('nav', 'href', { valueKey: 'href' })
    handleHome() {
      return { element: OverviewPage(w), href: 'home' };
    }

    @replaceChildrenLight({ valueKey: 'element' })
    @subscribeSwcAppRouteChange('/topics')
    @attribute('nav', 'href', { valueKey: 'href' })
    handleTopics() {
      return { element: TopicsPage(w), href: 'topics' };
    }

    @replaceChildrenLight({ valueKey: 'element' })
    @subscribeSwcAppRouteChange('/world')
    @attribute('nav', 'href', { valueKey: 'href' })
    handleWorld() {
      return { element: WorldPage(w), href: 'world' };
    }

    @subscribeSwcAppRouteChange({ order: -1 })
    @removeAttribute('nav', 'href')
    handleNavActive() {
      return undefined;
    }

    @replaceChildrenLight
    @subscribeSwcAppRouteChange(['/{tail:.*}'], { order: 999 })
    handle404(routerPathSet: RouterEventType) {
      return htmlFragment(`<p style="padding:32px">404 — 없는 경로: ${routerPathSet.path}</p>`, w.document);
    }

    @eventDelegateAll('[data-href]', 'click')
    @swcAppRouteGo
    onNavigate(@matchedElement el: Element) {
      return el.getAttribute('data-href');
    }

    @onConnectedBodyShadow
    render() {
      return `
      <style>
        :host { display: flex; flex-direction: column; min-height: 100vh; background: var(--bg); color: var(--text); font-family: var(--font-sans); }
        header { display: flex; gap: 10px; align-items: center; height: 56px; padding: 0 16px; background: #16161f; border-bottom: 1px solid var(--border); position: sticky; top: 0; z-index: 10; }
        .logo { font-weight: 800; font-size: 16px; cursor: pointer; letter-spacing: -0.01em; }
        .logo b { color: var(--brand); }
        nav { display: flex; gap: 4px; margin-left: 12px; }
        nav button { font: inherit; font-size: 13px; font-weight: 600; color: var(--muted); background: none; border: none; padding: 7px 14px; border-radius: 8px; cursor: pointer; }
        nav button:hover { color: var(--text); background: var(--surface2); }
        nav[href="home"] [data-href="/"], nav[href="topics"] [data-href="/topics"], nav[href="world"] [data-href="/world"] { color: var(--text); background: var(--brand-dim); }
        main { flex: 1; width: 100%; max-width: 1440px; margin: 0 auto; }
      </style>
      <header>
        <div class="logo" data-href="/">Lv2 <b>통제실</b></div>
        <nav><button data-href="/">개요</button><button data-href="/topics">토픽</button><button data-href="/world">3D World</button></nav>
      </header>
      <main><slot></slot></main>`;
    }
  }

  return w.customElements.whenDefined(tagName);
};

export default defineCenterRouter;
