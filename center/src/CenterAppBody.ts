import { createElement, type CreateElementConfig, elementDefine, property, SwcAppMixin, type SwcAppInterface } from '@dooboostore/simple-web-component';
import type { Subscription } from '@dooboostore/core';
import { TARGET_CHANGED } from './utils/ui';

export const centerAppBodyTagName = 'swc-app-center-body';

export const CenterAppBody = (w: Window, data?: CreateElementConfig) => {
  return createElement<SwcAppInterface & HTMLBodyElement>(w, centerAppBodyTagName, data);
};

export const defineCenterAppBody = async (w: Window) => {
  const existing = w.customElements.get(centerAppBodyTagName);
  if (existing) return existing;

  @elementDefine(centerAppBodyTagName, { window: w, extends: 'body' })
  class CenterAppBodyImpl extends SwcAppMixin(w.HTMLBodyElement) implements SwcAppInterface {
    // 현재 보고 있는 target id (debug 폴더 1개 = 1 ROS_DOMAIN_ID)
    @property
    declare target: string | null;
    private targetSubscription?: Subscription;

    override onConnected(): void {
      // @property hydration은 SSR 필터가 박은 window.__swc_hydration 스크립트가
      // 번들보다 먼저 실행되어 JS 프로퍼티로 직접 세팅 (data-hyd-* 어트리뷰트 방식은 폐기됨).
      // 네이티브 커스텀엘리먼트 업그레이드는 own property를 유지하므로 값이 살아남음.
      this.targetSubscription = this.observeMessage<string>(TARGET_CHANGED, { subject: 'behavior' }).subscribe((msg) => {
        this.target = msg.data ?? null;
      });
    }

    override onDisconnected(): void {
      this.targetSubscription?.unsubscribe();
      this.targetSubscription = undefined;
    }

    // connect() 완료 후. center는 로그인 같은 부트 조회가 없어서 할 일 없음 —
    // 추상 메서드라 빈 구현 (TS2515). target은 behavior 구독으로 들어옴.
    override async onSwcAppConnected(): Promise<void> {}
  }

  return w.customElements.whenDefined(centerAppBodyTagName);
};

export default defineCenterAppBody;
