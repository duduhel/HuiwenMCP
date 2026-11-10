// 单个 Qt Ability 的双向桥：统一协议收发和会话归属，不承担窗口或沙箱业务。
import nativeBridge, { HarmonySandboxDocument } from 'libhuiwen_native.so';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BridgeEvent, BridgeResult, WorkspaceHandlers, ScreenshotHandlers } from './BridgeTypes';

export class QtSessionBridge {
  private sessionId: number = 0;
  private workspace: WorkspaceHandlers | undefined = undefined;
  private screenshot: ScreenshotHandlers | undefined = undefined;

  // ArkTS Ability → Native：注册 mode／kind 对应的文档会话，handlers 接收 Qt 的具名请求。
  attachWorkspace(mode: string, kind: string, handlers: WorkspaceHandlers): void {
    this.workspace = handlers;
    this.attach(mode, kind);
  }

  // ArkTS Ability → Native：注册截图会话；cacheRoot 是已经取得访问权限的缓存目录。
  attachScreenshot(cacheRoot: string, handlers: ScreenshotHandlers): void {
    this.screenshot = handlers;
    this.attach('screenshot', '', cacheRoot);
  }

  // 保存 Native 返回的会话 ID；TSFN 回调在 ArkTS 线程执行，异步处理失败统一记日志。
  private attach(mode: string, kind: string, cacheRoot?: string): void {
    this.sessionId = nativeBridge.sessionAttach((json: string): void => {
      this.dispatch(json).catch((error: Error) => {
        hilog.error(0xFF00, 'QtSessionBridge', 'HUIWEN_BRIDGE_EVENT_FAILED: %{public}s', error.message);
      });
    }, mode, kind, cacheRoot);
  }

  // Ability 销毁后解除本实例的回调；先清空本地身份，Native 只注销相同 ID，不影响新宿主。
  detach(): void {
    const id = this.sessionId;
    this.sessionId = 0;
    this.workspace = undefined;
    this.screenshot = undefined;
    if (id !== 0) { nativeBridge.sessionDetach(id); }
  }

  // Qt → ArkTS 唯一协议入口：先核对会话，再调用业务处理器，不在此执行窗口或文件操作。
  private async dispatch(json: string): Promise<void> {
    const event = JSON.parse(json) as BridgeEvent;
    if (this.sessionId === 0 || event.sessionId !== this.sessionId) { return; }
    const workspace = this.workspace;
    if (workspace !== undefined) {
      switch (event.action) {
        case 'ready': await workspace.onReady(); return;
        case 'close': await workspace.onReturnHome(); return;
        case 'closeApproved': await workspace.onCloseApproval(true); return;
        case 'closeCancelled': await workspace.onCloseApproval(false); return;
        case 'closed': await workspace.onClosed(); return;
        case 'restoreResult': workspace.onRestoreResult(event.taskPath ?? '', event.error ?? ''); return;
        case 'pick': await workspace.onSelectDocument(event.kind ?? '', event.mode ?? ''); return;
        case 'open': await workspace.onOpenOutputLocation(event.path ?? ''); return;
        case 'openDirectory': await workspace.onOpenTaskDirectory(event.path ?? ''); return;
        case 'aboutDevice': await workspace.onOpenAboutDevice(); return;
      }
    }
    const screenshot = this.screenshot;
    if (screenshot !== undefined) {
      switch (event.action) {
        case 'downloadScreenshot':
          await screenshot.onDownload(event.path ?? '', event.error ?? '', event.displayId ?? 0); return;
        case 'colorCopied':
          await screenshot.onColorCopied(event.format ?? '', event.value ?? '', event.color ?? '',
            event.displayId ?? 0); return;
        case 'screenshotFinished':
          await screenshot.onFinished(event.ok ?? false, event.path ?? '', event.error ?? '', event.cached ?? false);
          return;
      }
    }
    throw new Error(`未知 Qt 会话事件：${event.action}`);
  }

  // ArkTS → Qt：返回系统选择的沙箱文档；取消传 null 且 error 为空，失败传错误，不覆盖原文档。
  completeDocumentSelection(document: HarmonySandboxDocument | null, error: string = ''): void {
    this.deliver({ action: 'pick', document: document ?? undefined, error: error });
  }

  // ArkTS → Qt：提交已校验的任务文档；失败传 null 与 error，Qt 完成加载后另回恢复结果。
  applyRestoredWorkspace(document: HarmonySandboxDocument | null, error: string = ''): void {
    this.deliver({ action: 'restore', document: document ?? undefined, error: error });
  }

  // ArkTS → Qt：反馈输出文件定位请求；error 为空只表示系统接受请求，不保证文件管理器已定位。
  completeOutputLocationOpen(error: string = ''): void {
    this.deliver({ action: 'open', error: error });
  }

  // ArkTS → Qt：反馈任务目录打开请求；error 为空表示系统接受请求。
  completeTaskDirectoryOpen(error: string = ''): void {
    this.deliver({ action: 'openDirectory', error: error });
  }

  // ArkTS → Qt：请求编辑器检查未保存内容，结果由 onCloseApproval 接收。
  requestWorkspaceCloseConfirmation(): void { this.deliver({ action: 'confirmClose' }); }
  // ArkTS → Qt：要求隐藏工作区并释放 CEF；收到 onClosed 后 Ability 再结束自身或应用。
  closeQtWorkspace(): void { this.deliver({ action: 'close' }); }
  // ArkTS → Qt：返回首页失败时撤销关闭，恢复编辑器的可操作状态。
  cancelWorkspaceClose(): void { this.deliver({ action: 'closeAborted' }); }

  // ArkTS → Qt：返回保存后的目标 path 与 error；成功后 Qt 才结束截图选区。
  completeScreenshotDownload(path: string, error: string): void {
    this.deliver({ action: 'downloadScreenshot', path: path, error: error });
  }

  // 仅为本实例的有效会话发送 JSON；Native 再校验 ID，并排队交给 Qt 线程。
  private deliver(result: BridgeResult): void {
    if (this.sessionId !== 0) {
      nativeBridge.sessionDeliverResult(this.sessionId, JSON.stringify(result));
    }
  }
}
