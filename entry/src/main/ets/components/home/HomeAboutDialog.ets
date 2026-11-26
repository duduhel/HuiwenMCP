// 关于弹窗：展示应用信息，并提供外部网页入口。
import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

@Component
export struct HomeAboutDialog {
  @Prop version: string = '';
  onClose: () => void = () => {};

  private async openWebsite(url: string): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      // 交由系统打开网页，不在应用内嵌入浏览器。
      await context.openLink(url);
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', '打开关于网页失败：%{public}s', (error as Error).message);
      this.getUIContext().getPromptAction().showToast({ message: `无法打开网页，请通过浏览器访问 ${url}` });
    }
  }

  build() {
    Stack({ alignContent: Alignment.TopEnd }) {
      Column({ space: 14 }) {
        Image($r('app.media.app_icon'))
          .width(64)
          .height(64)
          .draggable(false)
        Text('汇文 MCP')
          .fontSize(24)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
        Text(this.version === '' ? '版本信息暂不可用' : `版本 ${this.version}`)
          .fontSize(14)
          .fontColor($r('app.color.home_muted'))
        Text('PDF 与 Markdown 的阅读和转换工具')
          .fontSize(16)
          .fontColor($r('app.color.home_text'))
          .textAlign(TextAlign.Center)
        Text('汇集千文 · 连通万物\n支持文字型 PDF 转换为 Markdown，让阅读与整理更轻松。')
          .fontSize(14)
          .lineHeight(22)
          .fontColor($r('app.color.home_muted'))
          .textAlign(TextAlign.Center)
        Row({ space: 16 }) {
          Button('汇文官网', { type: ButtonType.Normal })
            .fontSize(14)
            .fontColor('#576b95')
            .backgroundColor(Color.Transparent)
            .padding({ left: 0, right: 0, top: 8, bottom: 8 })
            .accessibilityText('在外部浏览器打开汇文官网')
            .onClick(() => this.openWebsite('https://huiwenlab.com/'))
          Divider()
            .vertical(true)
            .height(16)
            .strokeWidth(1)
            .color('#D9D9D9')
          Button('隐私协议', { type: ButtonType.Normal })
            .fontSize(14)
            .fontColor('#576b95')
            .backgroundColor(Color.Transparent)
            .padding({ left: 0, right: 0, top: 8, bottom: 8 })
            .accessibilityText('在外部浏览器打开隐私协议')
            .onClick(() => this.openWebsite('https://huiwenlab.com/privacy/'))
        }
      }
      .width('100%')
      .padding({ top: 36, bottom: 32, left: 28, right: 28 })
      Button({ type: ButtonType.Normal }) {
        Image($r('app.media.home_close'))
          .width(18)
          .height(18)
          .draggable(false)
      }
      .width(32)
      .height(32)
      .padding(0)
      .margin(8)
      .backgroundColor(Color.Transparent)
      .borderRadius(8)
      .accessibilityText('关闭关于')
      .onClick(() => this.onClose())
    }
    .width('100%')
  }
}
