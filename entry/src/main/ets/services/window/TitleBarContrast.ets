// 标题栏试验性取色：整屏抓取只留在内存，读取两条窄像素行，不接触截图底图缓存。
import { screenshot, window } from '@kit.ArkUI';
import { abilityAccessCtrl, common } from '@kit.AbilityKit';
import { image } from '@kit.ImageKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

export class TitleBarContrast {
  private timer: number = -1;
  private stopped: boolean = false;
  private white: boolean | undefined = undefined;
  private measured: boolean = false;
  private context: common.Context;
  private target: window.Window;
  private apply: (white: boolean) => void;

  constructor(context: common.Context, target: window.Window, apply: (white: boolean) => void) {
    this.context = context;
    this.target = target;
    this.apply = apply;
    this.timer = setTimeout((): void => { this.tick(); }, 0);
  }

  stop(): void {
    this.stopped = true;
    clearTimeout(this.timer);
  }

  private publish(white: boolean): void {
    if (this.white !== white) {
      this.white = white;
      this.apply(white);
    }
  }

  private async tick(): Promise<void> {
    let pixels: image.PixelMap | undefined = undefined;
    try {
      if (this.stopped) { return; }
      if (!this.target.isFocused()) {
        // 失焦时窗口容器已是不透明侧栏底色，直接按已知底色配文字，无需抓屏。
        const color = this.context.resourceManager.getColorSync($r('app.color.home_sidebar').id);
        this.publish((((color >> 16) & 255) * 299 + ((color >> 8) & 255) * 587 +
          (color & 255) * 114) / 1000 < 128);
        return;
      }
      // 配色试验不主动弹权限申请；沿用用户已授予截图功能的权限。
      const access = abilityAccessCtrl.createAtManager();
      if (access.checkAccessTokenSync(this.context.applicationInfo.accessTokenId,
        'ohos.permission.CUSTOM_SCREEN_CAPTURE') !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        return;
      }
      const properties = this.target.getWindowProperties();
      const started = Date.now();
      pixels = await screenshot.capture({ displayId: properties.displayId });
      if (this.stopped || !this.target.isFocused()) { return; }
      const info = await pixels.getImageInfo();
      if (info.pixelFormat !== image.PixelMapFormat.RGBA_8888 &&
        info.pixelFormat !== image.PixelMapFormat.BGRA_8888 &&
        info.pixelFormat !== image.PixelMapFormat.ARGB_8888) {
        throw new Error(`不支持的取色像素格式：${info.pixelFormat}`);
      }
      const rect = properties.windowRect;
      const ui = this.target.getUIContext();
      // 原生 windowRect 是所在屏幕的局部物理坐标，不能再加虚拟桌面的屏幕偏移。
      const left = Math.max(0, Math.ceil(rect.left + ui.vp2px(60)));
      const right = Math.min(info.size.width, Math.floor(rect.left + rect.width - ui.vp2px(160)));
      const height = Math.min(rect.height, Math.round(ui.vp2px(37)));
      if (right <= left || rect.top < 0 || rect.top + height > info.size.height) { return; }
      let brightness = 0;
      for (const y of [Math.floor(rect.top + 2), Math.floor(rect.top + height - 3)]) {
        const bytes = new Uint8Array((right - left) * 4);
        await pixels.readPixels({ pixels: bytes.buffer, offset: 0, stride: bytes.byteLength,
          region: { x: left, y: y, size: { width: right - left, height: 1 } } });
        const red = info.pixelFormat === image.PixelMapFormat.BGRA_8888 ? 2 :
          info.pixelFormat === image.PixelMapFormat.ARGB_8888 ? 1 : 0;
        const green = info.pixelFormat === image.PixelMapFormat.ARGB_8888 ? 2 : 1;
        const blue = info.pixelFormat === image.PixelMapFormat.ARGB_8888 ? 3 : 2 - red;
        for (let i = 0; i < 6; i++) {
          const offset = Math.floor((right - left - 1) * (i + 0.5) / 6) * 4;
          brightness += (bytes[offset + red] * 299 + bytes[offset + green] * 587 +
            bytes[offset + blue] * 114) / 1000;
        }
      }
      if (this.stopped || !this.target.isFocused() ||
        this.target.getWindowProperties().displayId !== properties.displayId) { return; }
      brightness /= 12;
      // 100～145 为保持区，避免临界亮度导致文字反复闪色。
      const white = this.white === undefined ? brightness < 128 :
        this.white ? brightness < 145 : brightness < 100;
      if (!this.measured || white !== this.white) {
        hilog.info(0xFF00, 'TitleBarContrast',
          'HUIWEN_TITLE_CONTRAST: display=%{public}d brightness=%{public}.1f white=%{public}s elapsed=%{public}dms',
          properties.displayId, brightness, String(white), Date.now() - started);
        this.measured = true;
      }
      this.publish(white);
    } catch (error) {
      // 取色失败只停止本次窗口的试验，不干扰工作区或截图流程，也不循环报错。
      this.stopped = true;
      hilog.warn(0xFF00, 'TitleBarContrast', 'HUIWEN_TITLE_CONTRAST_FAILED: %{public}s', (error as Error).message);
    } finally {
      if (pixels) {
        await pixels.release().catch((error: Error): void => {
          this.stopped = true;
          hilog.warn(0xFF00, 'TitleBarContrast', 'HUIWEN_TITLE_CONTRAST_RELEASE_FAILED: %{public}s', error.message);
        });
      }
      // 完成本次后再计时，抓屏慢时不会叠加任务；实际周期为耗时 + 200ms。
      if (!this.stopped) { this.timer = setTimeout((): void => { this.tick(); }, 200); }
    }
  }
}
