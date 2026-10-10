/**
 * Global type augmentation for center SSR
 * All `any`: dom-parser's Window impl is a subset of DOM types,
 * so precise typing only causes conflicts. Build uses transpileOnly anyway.
 */
declare global {
  interface Window {
    HTMLElement: typeof HTMLElement;
    HTMLDivElement: typeof HTMLDivElement;
    HTMLButtonElement: typeof HTMLButtonElement;
    HTMLTemplateElement: typeof HTMLTemplateElement;
    HTMLAnchorElement: typeof HTMLAnchorElement;
    HTMLBodyElement: typeof HTMLBodyElement;
    CustomEvent: typeof CustomEvent;
    location: Location;
    document: Document;
    history: History;
  }
}

export {};
