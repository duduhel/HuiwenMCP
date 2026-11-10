// Qt 独立窗口入口；保留自己的 Context 处理文件选择，首页结束后仍可使用。
import { UIAbility, Want, AbilityConstant, OpenLinkOptions } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { window } from '@kit.ArkUI';
import { fileUri } from '@kit.CoreFileKit';
import hilog from '@ohos.hilog';
import qpa from 'libplugins_platforms_qopenharmony.so';
import nativeBridge, { HarmonySandboxDocument } from 'libhuiwen_native.so';
import { DocumentKind, DocumentMode, pickDocumentToSandbox } from '../utils/DocumentStore';
import { CefWorkspaceProxy } from '../utils/CefPlatform';
import { readSettings } from '../services/settings/AppSettings';
import { ensureWorkspaceAccess } from '../services/settings/WorkspaceStorage';
import { QtSessionBridge } from '../bridge/QtSessionBridge';
import { applyHomeWindowColors } from '../utils/HomeAppearance';

export default class QtWorkspaceAbility extends UIAbility {
  // QPA 读取这两个字段，加载应用共享库并调用其中的 main()。
  private launchApplication: string = 'libhuiwen_qt.so';
  private launchParams: string = '';
  private pickerBusy: boolean = false;
  private workspaceReady: boolean = false;
  private pendingTask: string = '';
  private mode: string = DocumentMode.Convert;
  private closeChecking: boolean = false;
  private kind: string = DocumentKind.PDF;
  private restoring: boolean = false;
  private destroyed: boolean = false;
  private closing: boolean = false;
  private exitOnClose: boolean = false;
  private session: QtSessionBridge = new QtSessionBridge();
  private cefProxy: CefWorkspaceProxy | undefined = undefined;
  // 首页确认显示后才让 Qt 释放浏览器；只消费本次关闭的一次回执。
  private homeReady = (error: string = ''): void => {
    this.context.getApplicationContext().eventHub.off('huiwen.home.ready', this.homeReady);
    if (this.destroyed) {
      return;
    }
    if (error !== '') {
      this.closing = false;
      this.session.cancelWorkspaceClose();
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_HOME_FAILED: %{public}s', error);
      return;
    }
    this.session.closeQtWorkspace();
  };

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    super.onCreate(want, launchParam);
    AppStorage.setOrCreate('documentWorkspaceActive', true);
    this.pendingTask = this.requestedTask(want);
    this.mode = String(want.parameters?.huiwenMode ?? DocumentMode.Convert);
    this.kind = String(want.parameters?.huiwenKind ?? DocumentKind.PDF);
  }

  private requestedTask(want: Want): string {
    const value = want.parameters?.huiwenTaskPath;
    return typeof value === 'string' ? value : '';
  }

  private async deliverPendingTask(): Promise<void> {
    if (this.restoring || this.destroyed || this.closing) {
      return;
    }
    const taskPath: string = this.pendingTask;
    if (taskPath === '') {
      return;
    }
    this.restoring = true;
    try {
      // 只传任务标识，实际文件路径由 C++ 重新校验；读取阶段不更新时间。
      const document: HarmonySandboxDocument = await nativeBridge.sandboxLoadWorkspace(
        await ensureWorkspaceAccess(this.context), taskPath);
      if (this.destroyed || this.closing || this.pendingTask !== taskPath) {
        return;
      }
      if (document.mode !== this.mode || document.kind !== this.kind) {
        throw new Error('任务类型与当前工作区不一致');
      }
      this.session.applyRestoredWorkspace(document);
    } catch (error) {
      if (!this.destroyed && !this.closing && this.pendingTask === taskPath) {
        this.pendingTask = '';
        this.restoring = false;
        const message: string = (error as Error).message;
        hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_RESTORE_FAILED: %{public}s', message);
        // 错误只交给当前 Qt 页面，不带回首页。
        this.session.applyRestoredWorkspace(null, message);
      }
    }
  }

  // QPA 调用本方法后写入 idName，并把存储传给固定的 pages/Index 承载页。
  newLocalStorage(): LocalStorage {
    return new LocalStorage();
  }

  private configureWindowTitleBar(target: window.Window): void {
    if (this.mode !== DocumentMode.Convert) {
      return;
    }
    try {
      target.setWindowDecorVisible(false);
      target.setWindowTitleButtonVisible(false, false, false);
    } catch (error) {
      hilog.warn(0xFF00, 'QtWorkspace', '隐藏系统窗口按钮失败：%{public}s',
        (error as BusinessError).message);
    }
    try {
      target.setWindowBackgroundColor('#00000000');
      applyHomeWindowColors(this.context, target);
    } catch (error) {
      hilog.warn(0xFF00, 'QtWorkspace', '设置工作区透明标题栏失败：%{public}s',
        (error as BusinessError).message);
    }
  }

  private async refreshWindowControls(): Promise<void> {
    if (this.mode !== DocumentMode.Convert || this.destroyed || this.closing) {
      return;
    }
    try {
      // QPA 创建的 Qt 窗口可能不同于 WindowStage 主窗，曝光后同步实际顶层窗口。
      const current = await window.getLastWindow(this.context);
      if (!this.destroyed && !this.closing) {
        this.configureWindowTitleBar(current);
      }
    } catch (error) {
      hilog.warn(0xFF00, 'QtWorkspace', '获取工作区窗口失败：%{public}s',
        (error as BusinessError).message);
    }
  }

  onForeground(): void {
    // 首次前台早于 QPA 内容就绪，首次配置由 workspaceShown 负责。
    if (this.workspaceReady) {
      this.refreshWindowControls();
    }
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    try {
      // 任一工作区均可原地切入 PDF 转化，统一准备内置业务包；命中缓存时不重复部署。
      nativeBridge.configurePython(this.context.resourceManager, this.context.cacheDir);
      this.cefProxy = new CefWorkspaceProxy(this.context, windowStage, (): void => {
        this.returnHome();
      });
      // 先挂接 QPA 宿主，再通知 Qt 创建对应的新窗体。
      qpa.startQtApplication(this);
      this.session.attachWorkspace(this.mode, this.kind, {
        onReady: (): Promise<void> => this.workspaceShown(),
        onReturnHome: (): Promise<void> => this.returnHome(),
        onCloseApproval: (approved: boolean): Promise<void> => this.closeApprovalReceived(approved),
        onClosed: (): Promise<void> => this.workspaceClosed(),
        onRestoreResult: (taskPath: string, error: string): void => this.restoreCompleted(taskPath, error),
        onSelectDocument: (kind: string, mode: string): Promise<void> => this.selectDocument(kind, mode),
        onOpenOutputLocation: (path: string): Promise<void> => this.openRequestedLocation(path, false),
        onOpenTaskDirectory: (path: string): Promise<void> => this.openRequestedLocation(path, true)
      });
      hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_START');
    } catch (error) {
      const message = (error as Error).message;
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_START_FAILED: %{public}s', message);
      this.context.terminateSelf().catch((failure: Error) => {
        hilog.error(0xFF00, 'QtWorkspace', '关闭失败的 Qt 窗口：%{public}s', failure.message);
      });
    }
  }

  private async workspaceShown(): Promise<void> {
    if (this.destroyed || this.closing) { return; }
    await this.refreshWindowControls();
    if (this.destroyed || this.closing) { return; }
    this.workspaceReady = true;
    hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_READY');
    // 窗口已显示即可结束原首页，文件恢复不参与窗口交接。
    this.context.getApplicationContext().eventHub.emit('huiwen.workspace.ready');
    await this.deliverPendingTask();
  }

  private async closeApprovalReceived(approved: boolean): Promise<void> {
    this.closeChecking = false;
    if (approved) { await this.returnHome(true); }
  }

  private async workspaceClosed(): Promise<void> {
    if (this.destroyed || !this.closing) { return; }
    if (this.exitOnClose) {
      await this.context.getApplicationContext().killAllProcesses(true);
    } else {
      await this.context.terminateSelf();
    }
  }

  private restoreCompleted(taskPath: string, error: string): void {
    if (this.destroyed || this.closing || !this.restoring || taskPath !== this.pendingTask) { return; }
    this.pendingTask = '';
    this.restoring = false;
    if (error !== '') {
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_RESTORE_FAILED: %{public}s', error);
    } else {
      hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_RESTORE_OK');
    }
  }

  private async selectDocument(requestKind: string, requestMode: string): Promise<void> {
    if (this.destroyed || this.closing || this.pickerBusy) { return; }
    this.pickerBusy = true;
    let document: HarmonySandboxDocument | null = null;
    let message = '';
    try {
      if (requestMode !== this.mode || requestKind !== this.kind) {
        throw new Error('选取请求与当前工作区类型不一致');
      }
      const kind = this.kind === DocumentKind.Word ? DocumentKind.Word :
        this.kind === DocumentKind.Markdown ? DocumentKind.Markdown : DocumentKind.PDF;
      const mode = this.mode === DocumentMode.Preview ? DocumentMode.Preview :
        this.mode === DocumentMode.Edit ? DocumentMode.Edit : DocumentMode.Convert;
      document = await pickDocumentToSandbox(this.context, kind, mode);
      if (document !== null) { hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_PICK_OK'); }
    } catch (error) {
      message = (error as Error).message;
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_PICK_FAILED: %{public}s', message);
    } finally {
      this.pickerBusy = false;
      if (!this.destroyed && !this.closing) { this.session.completeDocumentSelection(document, message); }
    }
  }

  private async openRequestedLocation(path: string, taskDirectory: boolean): Promise<void> {
    if (this.destroyed || this.closing) { return; }
    let message = '';
    try {
      if (taskDirectory) {
        const directory = nativeBridge.sandboxValidateTaskDirectory(await ensureWorkspaceAccess(this.context), path);
        await this.openFileManager(directory);
      } else {
        await this.openOutputLocation(path);
      }
    } catch (error) {
      message = (error as Error).message;
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_OPEN_FAILED: %{public}s', message);
    }
    if (this.destroyed || this.closing) { return; }
    if (taskDirectory) {
      this.session.completeTaskDirectoryOpen(message);
    } else {
      this.session.completeOutputLocationOpen(message);
    }
  }

  private async returnHome(approved: boolean = false): Promise<void> {
    if (this.closing || this.destroyed) {
      return;
    }
    // 当前是否在编辑由 Qt 判断，不能再依赖最初启动的任务模式。
    if (!approved) {
      if (this.closeChecking) {
        return;
      }
      this.closeChecking = true;
      this.session.requestWorkspaceCloseConfirmation();
      return;
    }
    this.closing = true;
    // 保留原关闭顺序，仅停止消费本次未完成的恢复结果。
    this.pendingTask = '';
    this.restoring = false;
    this.exitOnClose = readSettings(this.context).closeAction === 'exit';
    if (this.exitOnClose) {
      // 沿用 Qt/CEF 的释放回执，收到 closed 后才退出应用。
      this.session.closeQtWorkspace();
      return;
    }
    this.context.getApplicationContext().eventHub.on('huiwen.home.ready', this.homeReady);
    try {
      await this.context.startAbility({ bundleName: 'cn.com.HuiWenMCP', abilityName: 'EntryAbility' });
    } catch (error) {
      this.homeReady((error as Error).message);
    }
  }

  private async openOutputLocation(path: string): Promise<void> {
    const outputPath: string = nativeBridge.sandboxValidateOutputFile(
      await ensureWorkspaceAccess(this.context), path);
    await this.openFileManager(outputPath);
  }

  // 文件卡片传入已验证文件，目录菜单传入已验证目录，共用系统文件管理器入口。
  private async openFileManager(path: string): Promise<void> {
    const uri: string = fileUri.getUriFromPath(path);
    const options: OpenLinkOptions = {
      parameters: { fileUri: uri }
    };
    hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_OPEN_REQUEST: path=%{public}s, uri=%{public}s',
      path, uri);
    try {
      await this.context.openLink('filemanager://openDirectory', options);
      // Promise 完成只表示拉起请求成功，目录定位效果仍需真机确认。
      hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_OPEN_REQUEST_ACCEPTED');
    } catch (error) {
      const failure = error as BusinessError;
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_OPEN_FAILED: code=%{public}d, message=%{public}s',
        failure.code, failure.message);
      throw new Error(`无法打开文件目录（${failure.code}）：${failure.message}`);
    }
  }

  onDestroy(): void {
    AppStorage.setOrCreate('documentWorkspaceActive', false);
    this.destroyed = true;
    this.context.getApplicationContext().eventHub.off('huiwen.home.ready', this.homeReady);
    this.pendingTask = '';
    // QPA 在本方法返回后标记宿主已销毁；下一轮事件再通知 Qt 释放窗体，避免抢先删除宿主。
    setTimeout(() => this.session.detach(), 0);
    this.cefProxy?.dispose();
    this.cefProxy = undefined;
    // 首页已经显示，在工作区实际销毁后重读最近列表，不修改沙箱时间规则。
    this.context.getApplicationContext().eventHub.emit('huiwen.workspace.closed');
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    // 关闭中的旧任务不再向新首页回传错误。
    if (this.closing) {
      return;
    }
    if (this.workspaceReady) {
      this.context.getApplicationContext().eventHub.emit('huiwen.workspace.ready');
    }
    if (this.restoring || this.pickerBusy) {
      this.session.applyRestoredWorkspace(null, '工作区正在处理文件，请稍后重试');
      return;
    }
    // ArkTS 保留原任务身份，Qt 内部可切换操作模式；外部 Want 不能替换本次会话的原任务类型。
    if (String(want.parameters?.huiwenMode ?? DocumentMode.Convert) !== this.mode ||
      String(want.parameters?.huiwenKind ?? DocumentKind.PDF) !== this.kind) {
      this.session.applyRestoredWorkspace(null, '请先关闭当前工作区，再打开其他模式或文档类型');
      return;
    }
    qpa.handleOnNewWant(want, launchParam);
    this.pendingTask = this.requestedTask(want);
    if (this.workspaceReady) {
      this.deliverPendingTask();
    }
  }
}
