// 离线库及 DOCX 只读预览入口；只在 showPreview 收到文档后挂载。
import './source/src/styles/editor.css';
import './preview.css';
import * as React from 'react';
import { createRoot, type Root } from 'react-dom/client';
import { DocxEditor, type DocxImportError, type DocxEditorZoom } from './source/src/index';

export * from './source/src/index';
export * as core from './source/src/core';
export { React, createRoot };

declare global {
  interface Window {
    huiwenReportPreview?: (ready: boolean, error: string) => void;
    huiwenReportScroll?: (page: number, fraction: number, progress: number) => void;
    huiwenReportZoom?: (percent: number) => void;
  }
}

let previewRoot: Root | undefined;
let generation = 0;
let applyZoom: ((percent: number) => void) | undefined;
let disposePreview: (() => void) | undefined;

function scrollElement(): HTMLElement | null {
  return document.querySelector<HTMLElement>('.docx-editor-editor');
}

export function scrollPosition() {
  const element = scrollElement();
  const maximum = element ? element.scrollHeight - element.clientHeight : 0;
  const progress = maximum > 0 ? Math.max(0, Math.min(1, element!.scrollTop / maximum)) : 0;
  return { page: 0, fraction: 0, progress };
}

function reportPosition() {
  const position = scrollPosition();
  window.huiwenReportScroll?.(position.page, position.fraction, position.progress);
}

export function setScrollPosition(_page: number, _fraction: number, progress: number) {
  const element = scrollElement();
  if (element && Number.isFinite(progress)) {
    element.scrollTop = Math.max(0, Math.min(1, progress)) *
      Math.max(0, element.scrollHeight - element.clientHeight);
  }
}

function PreviewError({ error }: { error: DocxImportError }) {
  const message = `无法预览 DOCX：${String(error.message).slice(0, 512)}`;
  React.useEffect(() => { window.huiwenReportPreview?.(false, message); }, [message]);
  return React.createElement('p', { className: 'word-preview-error', role: 'alert' }, message);
}

export function setZoomPercent(percent: number): void {
  if (Number.isFinite(percent)) {
    applyZoom?.(Math.max(25, Math.min(200, percent)));
  }
}

export function showPreview(encoded: string) {
  const container = document.getElementById('word-body');
  if (!container) throw new Error('缺少 Word 预览容器');
  const bytes = Uint8Array.from(atob(encoded), (character) => character.charCodeAt(0));
  disposePreview?.();
  const current = ++generation;
  previewRoot ??= createRoot(container);
  let zoom: DocxEditorZoom = 'fit-width';
  let viewport: HTMLElement | null = null;
  let box: HTMLElement | null = null;
  let observer: ResizeObserver | undefined;
  let frame = 0;
  let centerY: number | undefined;
  const scale = (): number => {
    const workspace = container.querySelector<HTMLElement>('.docx-editor-workspace');
    return workspace ? Number.parseFloat(getComputedStyle(workspace)
      .getPropertyValue('--docx-editor-zoom')) || 1 : 1;
  };
  const currentPercent = (): number => (typeof zoom === 'number' ? zoom : scale()) * 100;
  let pinching = false;
  let pinchDistance = 0;
  let pinchPercent = 100;
  const touchDistance = (event: TouchEvent): number => Math.hypot(
    event.touches[0].clientX - event.touches[1].clientX,
    event.touches[0].clientY - event.touches[1].clientY);
  const onTouchStart = (event: TouchEvent): void => {
    if (event.touches.length !== 2) return;
    event.preventDefault();
    pinching = true;
    pinchDistance = touchDistance(event);
    pinchPercent = currentPercent();
  };
  const onTouchMove = (event: TouchEvent): void => {
    if (!pinching) return;
    event.preventDefault();
    if (event.touches.length === 2 && pinchDistance > 0) {
      setZoomPercent(pinchPercent * touchDistance(event) / pinchDistance);
    }
  };
  const onTouchEnd = (event: TouchEvent): void => {
    if (!pinching) return;
    if (event.cancelable) event.preventDefault();
    // 捏合后剩余单指不接续滚动，全部松手或取消后才接受下一次滑动。
    if (event.type === 'touchcancel' || event.touches.length === 0) pinching = false;
  };
  const onWheel = (event: WheelEvent): void => {
    if (!event.ctrlKey) return;
    // Chromium 将触控板捏合转换为 Ctrl+Wheel；拦截默认页面缩放。
    event.preventDefault();
    const unit = event.deltaMode === WheelEvent.DOM_DELTA_LINE ? 16 :
      event.deltaMode === WheelEvent.DOM_DELTA_PAGE ? viewport!.clientHeight : 1;
    setZoomPercent(currentPercent() * Math.exp(Math.max(-4, Math.min(4, -event.deltaY * unit / 100))));
  };
  const rememberCenter = (): void => {
    if (viewport && box) {
      centerY = (viewport.scrollTop + viewport.clientHeight / 2 - box.offsetTop) / scale();
    }
  };
  const onScroll = (): void => {
    if (frame === 0) rememberCenter();
    reportPosition();
  };
  const alignPreview = (): void => {
    if (frame !== 0) return;
    frame = requestAnimationFrame(() => {
      frame = 0;
      if (current !== generation || !viewport || !box) return;
      const factor = scale();
      viewport.scrollLeft = Math.max(0, viewport.scrollWidth - viewport.clientWidth) / 2;
      if (centerY !== undefined) {
        viewport.scrollTop = box.offsetTop + centerY * factor - viewport.clientHeight / 2;
      }
      rememberCenter();
      window.huiwenReportZoom?.(Math.round(factor * 100));
      reportPosition();
    });
  };
  const onReady = (): void => {
    requestAnimationFrame(() => {
      if (current !== generation || viewport) return;
      viewport = scrollElement();
      box = container.querySelector<HTMLElement>('.docx-editor-page-box');
      if (!viewport || !box) return;
      viewport.addEventListener('scroll', onScroll, { passive: true });
      viewport.addEventListener('wheel', onWheel, { passive: false });
      viewport.addEventListener('touchstart', onTouchStart, { passive: false });
      viewport.addEventListener('touchmove', onTouchMove, { passive: false });
      viewport.addEventListener('touchend', onTouchEnd, { passive: false });
      viewport.addEventListener('touchcancel', onTouchEnd, { passive: false });
      observer = new ResizeObserver(alignPreview);
      observer.observe(viewport);
      observer.observe(box);
      window.huiwenReportPreview?.(true, '');
      alignPreview();
    });
  };
  const render = (): void => previewRoot!.render(React.createElement(DocxEditor, {
    key: current,
    document: bytes,
    mode: { kind: 'readOnly' },
    zoom,
    showPageGuides: true,
    renderImportError: (error) => React.createElement(PreviewError, { error }),
    onReady
  }));
  applyZoom = (percent: number): void => {
    rememberCenter();
    zoom = percent / 100;
    // 相同 key 和 bytes 只更新引擎缩放属性，不重新打开 DOCX 或创建 CEF。
    render();
  };
  disposePreview = (): void => {
    cancelAnimationFrame(frame);
    observer?.disconnect();
    viewport?.removeEventListener('scroll', onScroll);
    viewport?.removeEventListener('wheel', onWheel);
    viewport?.removeEventListener('touchstart', onTouchStart);
    viewport?.removeEventListener('touchmove', onTouchMove);
    viewport?.removeEventListener('touchend', onTouchEnd);
    viewport?.removeEventListener('touchcancel', onTouchEnd);
    applyZoom = undefined;
  };
  render();
}
