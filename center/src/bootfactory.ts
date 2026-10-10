import swcRegister, { type SwcAppInterface } from '@dooboostore/simple-web-component';
import { componentFactories } from './component';
import centerAppBodyFactory from './CenterAppBody';
import { pageFactories } from './pages';
import { serviceFactories } from './services';
import { ValidUtils } from '@dooboostore/core-web';
import { UrlUtils } from '@dooboostore/core';

export default async (w: Window, other: Map<symbol, any> | ((c: symbol) => void)[] = [], path?: string) => {
  const container = Symbol('container');
  const isServer = !ValidUtils.isBrowser();

  const otherServiceFactories = Array.isArray(other) ? other : [];
  const otherInstanceSim = !Array.isArray(other) ? other : new Map<symbol, any>();
  [...otherServiceFactories, ...serviceFactories].forEach((it) => it(container));

  await swcRegister(w, [centerAppBodyFactory]);

  const appElement = w.document.querySelector('#app') as SwcAppInterface | null;
  if (!appElement || typeof appElement.connect !== 'function') {
    throw new Error('[center] Failed to initialize SWC App: #app element not found.');
  }

  const currentPath = path ?? UrlUtils.getUrlPath(w.location) ?? '/';

  return new Promise<{ app: SwcAppInterface }>((resolve, reject) => {
    let connected = false;
    let routed = false;
    const done = () => {
      if (connected && routed) resolve({ app: appElement });
    };

    let timeout: NodeJS.Timeout | undefined;
    if (isServer) {
      timeout = setTimeout(() => {
        console.warn('[center] SSR Timeout reached, resolving with current state');
        resolve({ app: appElement });
      }, 3000);
    }

    try {
      appElement.connect({
        path: currentPath,
        routeType: 'path',
        ssr: true,
        container: container,
        window: w,
        otherInstanceSim: otherInstanceSim,
        onStartedLazyDefineComponent: [...componentFactories, ...pageFactories],
        onChildrenConnectedDone: () => {
          connected = true;
          done();
        },
        onChildrenRouteChanged: () => {
          routed = true;
          done();
        }
      });
    } catch (error) {
      if (timeout) clearTimeout(timeout);
      reject(error);
    }
  });
};
