// 汇文截图独立 Qt 宿主；首页保留，截图结束后由 QPA 销毁覆盖窗口。
import { UIAbility, Want, AbilityConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BusinessError } from '@kit.BasicServicesKit';
import qpa from 'libplugins_platforms_qopenharmony.so';
import nativeBridge from 'libhuiwen_native.so';
import { prepareScreenshotToast } from '../services/screenshot/ScreenToast';
import { finishScreenshot } from '../services/screenshot/ScreenshotService';
import { downloadScreenshot, rememberColor, rememberScreenshot } from '../services/screenshot/ScreenshotHistory';
import { screenshotCacheRoot } from '../services/settings/AppSettings';
import { QtSessionBridge } from '../bridge/QtSessionBridge';

export default class QtScreenshotAbility extends UIAbility {
  // 与工作区共用 Qt 运行时，启动参数由现有 NAPI 会话桥区分。
  private launchApplication: string = 'libhuiwen_qt.so';
  private launchParams: string = '';
  private contentReady: () => void = () => {};
  private destroyed: boolean = false;
  private session: QtSessionBridge = new QtSessionBridge();

  newLocalStorage(): LocalStorage {
    const storage = new LocalStorage();
    storage.setOrCreate('huiwenScreenshot', true);
    return storage;
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    const events = this.context.getApplicationContext().eventHub;
    this.contentReady = (): void => {
      events.off('huiwen.screenshot.contentReady', this.contentReady);
      this.startScreenshot(windowStage);
    };
    events.on('huiwen.screenshot.contentReady', this.contentReady);
    try {
      // 首页已抓屏；QPA 加载 Index 后再通知 Qt 取用起窗前的底图。
      qpa.startQtApplication(this);
    } catch (error) {
      this.failStart(error as Error);
    }
  }

  private async startScreenshot(windowStage: window.WindowStage): Promise<void> {
    try {
      // 此时 UIContent 已创建，可以安全设置原生窗口背景和装饰。
      const mainWindow = windowStage.getMainWindowSync();
      mainWindow.setWindowBackgroundColor('#00000000');
      mainWindow.setWindowDecorVisible(false);
      // 系统可能保留上次全屏状态；在 Qt 准备窗创建前先恢复普通窗口。
      if (mainWindow.getWindowStatus() !== window.WindowStatusType.FLOATING) {
        await mainWindow.recover();
      }
      if (this.destroyed) { return; }
      // 不注册 CEF 代理；业务处理不再解析 JSON，会话归属由桥接层负责。
      this.session.attachScreenshot(screenshotCacheRoot(this.context), {
        onPrepareScreen: (displayId: number): Promise<void> =>
          this.prepareScreenshotScreen(windowStage, displayId),
        onDownload: (path: string, error: string, displayId: number): Promise<void> =>
          this.download(path, error, displayId),
        onColorCopied: (format: string, value: string, color: string, displayId: number): Promise<void> =>
          this.colorCopied(format, value, color, displayId),
        onFinished: (ok: boolean, path: string, error: string, cached: boolean): Promise<void> =>
          this.screenshotFinished(windowStage, ok, path, error, cached)
      });
    } catch (error) {
      this.failStart(error as Error);
    }
  }

  private async prepareScreenshotScreen(windowStage: window.WindowStage, displayId: number): Promise<void> {
    let error = '';
    try {
      const current = windowStage.getMainWindowSync();
      // moveWindowToGlobal 只接受普通窗口；逐步等待原生操作，Qt 不再并发改尺寸和状态。
      if (current.getWindowStatus() !== window.WindowStatusType.FLOATING) {
        await current.recover();
      }
      if (this.destroyed) { return; }
      await current.moveWindowToGlobal(0, 0, { displayId: displayId });
      if (this.destroyed) { return; }
      await current.maximize(window.MaximizePresentation.ENTER_IMMERSIVE_DISABLE_TITLE_AND_DOCK_HOVER);
      if (this.destroyed) { return; }
      const properties = current.getWindowProperties();
      if (properties.displayId !== displayId) {
        throw new Error(`截图屏幕不匹配：请求 ${displayId}，实际 ${properties.displayId}`);
      }
      hilog.info(0xFF00, 'HuiwenScreenshot',
        'HUIWEN_SCREENSHOT_SCREEN_READY: display=%{public}d size=%{public}dx%{public}d',
        displayId, properties.windowRect.width, properties.windowRect.height);
    } catch (failure) {
      error = (failure as Error).message;
      hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_SCREEN_FAILED: %{public}s', error);
    }
    if (!this.destroyed) { this.session.completeScreenshotScreen(error); }
  }

  private async colorCopied(format: string, value: string, color: string, displayId: number): Promise<void> {
    const context = this.context;
    // Qt 已将截图窗口切换为两秒提示；这里只入库并关闭窗口输入，不依赖首页。
    rememberColor(context.getApplicationContext(), value, color).catch((error: Error) => {
      hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_COLOR_HISTORY_FAILED: %{public}s', error.message);
    });
    try {
      await prepareScreenshotToast(context);
    } catch (failure) {
      const error = failure as BusinessError;
      hilog.warn(0xFF00, 'HuiwenScreenshot',
        'HUIWEN_SCREEN_TOAST_FAILED: code=%{public}d message=%{public}s', error.code ?? -1, error.message);
    }
  }

  private async screenshotFinished(windowStage: window.WindowStage, ok: boolean, path: string,
    error: string, cached: boolean): Promise<void> {
    if (error !== '') {
      hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_FAILED: %{public}s', error);
      windowStage.getMainWindowSync().getUIContext().getPromptAction().showToast({ message: error });
      // 抓屏失败可能没有 Qt 覆盖窗口，显式结束空宿主。
      await this.context.terminateSelf();
    } else {
      hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_FINISHED: %{public}s', path);
      if (ok && path !== '') {
        await rememberScreenshot(this.context.getApplicationContext(), path, cached);
      }
    }
  }

  private async download(sourcePath: string, sourceError: string, displayId: number): Promise<void> {
    let path = '';
    let error = '';
    try {
      if (sourceError !== '') { throw new Error(sourceError); }
      path = await downloadScreenshot(this.context, sourcePath);
      hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_DOWNLOADED: %{public}s', path);
    } catch (failure) {
      error = (failure as Error).message;
      hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_DOWNLOAD_FAILED: %{public}s', error);
    }
    if (this.destroyed) { return; }
    // 用户可能在异步保存期间取消截图；旧宿主不能向下一次 Qt 会话发送回包。
    if (!this.destroyed) {
      this.session.completeScreenshotDownload(path, error);
      try {
        if (error === '') {
          await prepareScreenshotToast(this.context);
        } else {
          const current = await window.getLastWindow(this.context);
          current.getUIContext().getPromptAction().showToast({ message: error });
        }
      } catch (failure) {
        hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREEN_TOAST_FAILED: %{public}s', (failure as Error).message);
      }
    }
  }

  private failStart(error: Error): void {
    finishScreenshot();
    nativeBridge.screenshotReleaseCapture();
    this.context.getApplicationContext().eventHub.off('huiwen.screenshot.contentReady', this.contentReady);
    hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_START_FAILED: %{public}s', error.message);
    this.context.terminateSelf().catch((failure: Error) => console.warn(failure.message));
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    qpa.handleOnNewWant(want, launchParam);
  }

  onDestroy(): void {
    this.destroyed = true;
    finishScreenshot();
    nativeBridge.screenshotReleaseCapture();
    this.context.getApplicationContext().eventHub.off('huiwen.screenshot.contentReady', this.contentReady);
    // 等 QPA 标记宿主已销毁后，再解除本次 Qt 会话，沿用工作区的释放顺序。
    setTimeout(() => this.session.detach(), 0);
  }
}
