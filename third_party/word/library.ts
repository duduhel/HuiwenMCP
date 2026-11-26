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
let applyZoom: ((percent: number, x?: number, y?: number) => void) | undefined;
let beginPreviewZoom: ((x?: number, y?: number) => void) | undefined;
let endPreviewZoom: (() => void) | undefined;
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

export function beginZoom(x?: number, y?: number): void {
  beginPreviewZoom?.(x, y);
}

export function endZoom(): void {
  endPreviewZoom?.();
}

export function setZoomPercent(percent: number, x?: number, y?: number): void {
  if (Number.isFinite(percent)) {
    applyZoom?.(Math.max(25, Math.min(200, percent)), x, y);
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
  let centerY: number | undefined;
  let zooming = false;
  let zoomAnchor: { page: number; fraction: number; viewportY: number } | undefined;
  const scale = (): number => {
    const workspace = container.querySelector<HTMLElement>('.docx-editor-workspace');
    return workspace ? Number.parseFloat(getComputedStyle(workspace)
      .getPropertyValue('--docx-editor-zoom')) || 1 : 1;
  };
  const currentPercent = (): number => (typeof zoom === 'number' ? zoom : scale()) * 100;
  const pageRects = (): Array<{ top: number; bottom: number; left: number; right: number }> => {
    const guides = container.querySelector<HTMLElement>('.docx-editor-page-guides');
    const bounds = (guides ?? box)?.getBoundingClientRect();
    if (!bounds) return [];
    const marks = Array.from(guides?.querySelectorAll<HTMLElement>(
      '.docx-editor-page-split, .docx-editor-page-crossed') ?? [])
      .map((mark) => mark.getBoundingClientRect()).sort((a, b) => a.top - b.top);
    const pages: Array<{ top: number; bottom: number; left: number; right: number }> = [];
    let top = bounds.top;
    for (const mark of marks) {
      pages.push({ top, bottom: mark.top, left: bounds.left, right: bounds.right });
      top = mark.bottom;
    }
    pages.push({ top, bottom: bounds.bottom, left: bounds.left, right: bounds.right });
    return pages;
  };
  const rememberZoomAnchor = (x?: number, y?: number): void => {
    if (!viewport) return;
    zoomAnchor = undefined;
    const visible = viewport.getBoundingClientRect();
    let largestArea = 0;
    pageRects().forEach((page, index) => {
      const top = Math.max(page.top, visible.top);
      const bottom = Math.min(page.bottom, visible.bottom);
      const width = Math.max(0, Math.min(page.right, visible.right) - Math.max(page.left, visible.left));
      const area = Math.max(0, bottom - top) * width;
      const underPointer = x !== undefined && y !== undefined &&
        x >= Math.max(page.left, visible.left) && x < Math.min(page.right, visible.right) &&
        y >= top && y < bottom;
      // 鼠标命中的页优先；否则选择当前可见面积最大的页。
      if (page.bottom <= page.top || area <= 0 || (!underPointer && area <= largestArea)) return;
      // 鼠标只选页；以该页中心为不动点，页面未完整显示时取可见部分中心。
      const anchorY = (top + bottom) / 2;
      zoomAnchor = {
        page: index,
        fraction: (anchorY - page.top) / (page.bottom - page.top),
        viewportY: anchorY - visible.top
      };
      largestArea = underPointer ? Infinity : area;
    });
  };
  const rememberRelativeAnchor = (x?: number, y?: number): void => {
    rememberZoomAnchor(x === undefined ? undefined : x * window.innerWidth,
      y === undefined ? undefined : y * window.innerHeight);
  };
  beginPreviewZoom = (x?: number, y?: number): void => {
    rememberRelativeAnchor(x, y);
    zooming = true;
  };
  endPreviewZoom = (): void => {
    zooming = false;
    if (Math.abs(scale() * 100 - currentPercent()) < 0.01) zoomAnchor = undefined;
  };
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
    rememberZoomAnchor((event.touches[0].clientX + event.touches[1].clientX) / 2,
      (event.touches[0].clientY + event.touches[1].clientY) / 2);
    zooming = true;
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
    if (event.type === 'touchcancel' || event.touches.length === 0) {
      pinching = false;
      endZoom();
    }
  };
  let lastWheelTime = -Infinity;
  const onWheel = (event: WheelEvent): void => {
    if (!event.ctrlKey) {
      endZoom();
      return;
    }
    // Chromium 将触控板捏合转换为 Ctrl+Wheel；拦截默认页面缩放。
    event.preventDefault();
    if (event.deltaY === 0) {
      endZoom();
      lastWheelTime = -Infinity;
      return;
    }
    // Wheel 没有公开的手势阶段；间隔超过 200ms 视为下一段输入，不增加等待定时器。
    if (event.timeStamp - lastWheelTime > 200 || !zooming) {
      rememberZoomAnchor(event.clientX, event.clientY);
      zooming = true;
    }
    lastWheelTime = event.timeStamp;
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
    if (!zoomAnchor) rememberCenter();
    reportPosition();
  };
  const alignPreview = (): void => {
    if (current !== generation || !viewport || !box) return;
    const factor = scale();
    if (typeof zoom === 'number' && Math.abs(factor - zoom) > 0.000001) return;
    const left = Math.max(0, viewport.scrollWidth - viewport.clientWidth) / 2;
    let top = viewport.scrollTop;
    const page = zoomAnchor ? pageRects()[zoomAnchor.page] : undefined;
    if (page && zoomAnchor) {
      const target = page.top + zoomAnchor.fraction * (page.bottom - page.top);
      top += target - viewport.getBoundingClientRect().top -
        Math.min(zoomAnchor.viewportY, viewport.clientHeight);
    } else if (centerY !== undefined) {
      top = box.offsetTop + centerY * factor - viewport.clientHeight / 2;
    }
    // 引擎已更新纸张占位高度；同次布局提交内同步位置，不等待下一帧回滚。
    viewport.scrollLeft = left;
    viewport.scrollTop = top;
    rememberCenter();
    if (!zooming) zoomAnchor = undefined;
    window.huiwenReportZoom?.(Math.round(factor * 100));
    reportPosition();
  };
  const onReady = (): void => {
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
  };
  // 仅用于安排绘制前的布局回调，不增加 DOM 容器；类型在本次打开期间保持稳定。
  const Preview = ({ value }: { value: DocxEditorZoom }): React.ReactElement => {
    React.useLayoutEffect(alignPreview, [value]);
    return React.createElement(DocxEditor, {
      document: bytes,
      mode: { kind: 'readOnly' },
      zoom: value,
      showPageGuides: true,
      renderImportError: (error) => React.createElement(PreviewError, { error }),
      onReady
    });
  };
  const render = (): void => previewRoot!.render(React.createElement(Preview, {
    key: current, value: zoom
  }));
  applyZoom = (percent: number, x?: number, y?: number): void => {
    if (Math.abs(currentPercent() - percent) < 0.000001) return;
    if (!zooming && !zoomAnchor) rememberRelativeAnchor(x, y);
    rememberCenter();
    zoom = percent / 100;
    // 相同 key 和 bytes 只更新引擎缩放属性，不重新打开 DOCX 或创建 CEF。
    render();
  };
  disposePreview = (): void => {
    observer?.disconnect();
    viewport?.removeEventListener('scroll', onScroll);
    viewport?.removeEventListener('wheel', onWheel);
    viewport?.removeEventListener('touchstart', onTouchStart);
    viewport?.removeEventListener('touchmove', onTouchMove);
    viewport?.removeEventListener('touchend', onTouchEnd);
    viewport?.removeEventListener('touchcancel', onTouchEnd);
    applyZoom = undefined;
    beginPreviewZoom = undefined;
    endPreviewZoom = undefined;
  };
  render();
}
