// 提示复用 Qt 截图普通窗口；这里只关闭原生输入热区，不创建或置顶任何悬浮窗。
import { common } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';

export async function prepareScreenshotToast(context: common.UIAbilityContext): Promise<void> {
  const current = await window.getLastWindow(context);
  await current.setWindowTouchable(false);
  await current.setWindowFocusable(false);
}
