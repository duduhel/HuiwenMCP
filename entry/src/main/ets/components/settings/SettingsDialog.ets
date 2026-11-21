// 设置页面仅协调选项与操作；目录维护及快捷键注册分别由 service 管理。
import { common } from '@kit.AbilityKit';
import { HomeDialog } from '../common/HomeDialog';
import { ShortcutInput } from './ShortcutInput';
import { defaultSettings, readSettings, saveSettings, ScreenshotShortcut, TitleTextColor, workspaceRoot,
  screenshotCacheRoot, screenshotDownloadRoot } from '../../services/settings/AppSettings';
import { chooseWorkspaceLocation, cleanWorkspaceCache, openWorkspaceLocation,
  restoreWorkspaceLocation } from '../../services/settings/WorkspaceStorage';
import { applyScreenshotShortcuts, CANCEL_SHORTCUT_RECORDING,
  finishShortcutRecording } from '../../services/screenshot/ScreenshotService';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { changeScreenshotDirectory, openScreenshotDirectory } from '../../services/screenshot/ScreenshotHistory';
import { closeScreenshotPreview } from '../../services/screenshot/ScreenshotPreview';
import { requestScreenCapturePermission } from '../../services/screenshot/ScreenCapturePermission';

const TITLE_COLOR_HELP: string = '跟随壁纸：需要屏幕捕获权限，根据透明标题栏后方的背景明暗自动调整文字颜色，' +
  '取色画面仅在本机处理，不保存、不上传。未授权时可点击该选项申请。\n\n' +
  '黑色／白色：固定文字颜色，不进行取色，可减少资源占用。\n\n' +
  '为获得更好的使用体验，建议您选择浅色壁纸。';

@Component
export struct SettingsDialog {
  @Prop dialogWidth: number = 660;
  @Prop contentHeight: number = 480;
  @State storagePath: string = '';
  @State screenshotCachePath: string = '';
  @State screenshotDownloadPath: string = '';
  @State recentLimit: number = 3;
  @State closeAction: string = 'home';
  @State titleTextColor: TitleTextColor = TitleTextColor.Black;
  @State colorHelpVisible: boolean = false;
  @State hiddenShortcut: ScreenshotShortcut = defaultSettings().hiddenShortcut;
  @State screenShortcut: ScreenshotShortcut = defaultSettings().screenShortcut;
  @State busy: boolean = false;
  @State message: string = '';
  onCloseRequested: () => void = () => {};

  aboutToAppear(): void {
    this.reload();
  }

  private context(): common.UIAbilityContext {
    return this.getUIContext().getHostContext() as common.UIAbilityContext;
  }

  private reload(): void {
    const context = this.context();
    const settings = readSettings(context);
    this.storagePath = workspaceRoot(context);
    this.screenshotCachePath = screenshotCacheRoot(context);
    this.screenshotDownloadPath = screenshotDownloadRoot(context);
    this.recentLimit = settings.recentLimit;
    this.closeAction = settings.closeAction;
    this.titleTextColor = settings.titleTextColor;
    this.hiddenShortcut = settings.hiddenShortcut;
    this.screenShortcut = settings.screenShortcut;
  }

  private async saveChoice(field: string, value: string): Promise<void> {
    this.busy = true;
    this.message = '';
    try {
      const settings = readSettings(this.context());
      if (field === 'recentLimit') {
        settings.recentLimit = Number(value);
      } else {
        settings.closeAction = value;
      }
      await saveSettings(this.context(), settings);
      this.reload();
    } catch (error) {
      this.message = (error as Error).message;
    } finally {
      this.busy = false;
    }
  }

  private async saveTitleTextColor(mode: TitleTextColor): Promise<void> {
    if (this.busy) { return; }
    this.busy = true;
    this.message = '';
    try {
      if (mode === TitleTextColor.Follow) {
        // 确认或授权完成前保持黑色，取消及拒绝均不启动取色。
        const settings = readSettings(this.context());
        settings.titleTextColor = TitleTextColor.Black;
        await saveSettings(this.context(), settings);
        this.reload();
        const confirmed: boolean = await new Promise<boolean>((resolve): void => {
          this.getUIContext().showAlertDialog({
            title: '启用跟随壁纸？',
            message: '需要屏幕捕获权限，以检测透明标题栏后方的背景明暗。取色画面仅在本机内存中处理，不保存、不上传。',
            primaryButton: { value: '取消', action: (): void => { resolve(false); } },
            secondaryButton: { value: '确认开启', action: (): void => { resolve(true); } },
            cancel: (): void => { resolve(false); }
          });
        });
        if (!confirmed) { return; }
        if (!await requestScreenCapturePermission(this.context())) {
          this.message = '未获得屏幕捕获权限，已使用黑色文字。';
          return;
        }
      }
      const settings = readSettings(this.context());
      settings.titleTextColor = mode;
      await saveSettings(this.context(), settings);
    } catch (error) {
      this.message = (error as Error).message;
    } finally {
      this.reload();
      this.busy = false;
    }
  }

  private async saveShortcut(hidden: boolean, shortcut: ScreenshotShortcut): Promise<void> {
    this.busy = true;
    this.message = '';
    const previous = readSettings(this.context());
    try {
      const settings = readSettings(this.context());
      if (hidden) {
        settings.hiddenShortcut = shortcut;
      } else {
        settings.screenShortcut = shortcut;
      }
      applyScreenshotShortcuts(settings);
      await saveSettings(this.context(), settings);
      this.reload();
      hilog.info(0xFF00, 'HuiwenSettings', 'HUIWEN_SHORTCUT_SAVED: action=%{public}s modifier=%{public}d key=%{public}d',
        hidden ? 'hidden' : 'screen', shortcut.modifier, shortcut.key);
    } catch (error) {
      try {
        applyScreenshotShortcuts(previous);
      } catch (failure) {
        hilog.error(0xFF00, 'HuiwenSettings', 'HUIWEN_SHORTCUT_RESTORE_FAILED: %{public}s',
          (failure as Error).message);
      }
      this.message = (error as Error).message;
      hilog.error(0xFF00, 'HuiwenSettings', 'HUIWEN_SHORTCUT_SAVE_FAILED: %{public}s', this.message);
      this.getUIContext().getPromptAction().showToast({ message: this.message });
    } finally {
      // 新组合或失败回滚已完成，只解除录入屏蔽，不重新订阅旧配置。
      finishShortcutRecording();
      this.busy = false;
    }
  }

  private async changeLocation(restore: boolean): Promise<void> {
    this.busy = true;
    AppStorage.setOrCreate('storageBusy', true);
    this.message = '正在迁移工作区，请勿关闭应用…';
    try {
      if (restore) {
        await restoreWorkspaceLocation(this.context());
      } else if (!await chooseWorkspaceLocation(this.context())) {
        this.message = '';
        return;
      }
      this.message = '存储位置已更新';
    } catch (error) {
      this.message = (error as Error).message;
    } finally {
      this.reload();
      this.busy = false;
      AppStorage.setOrCreate('storageBusy', false);
    }
  }

  private async browseLocation(): Promise<void> {
    this.message = '';
    try {
      await openWorkspaceLocation(this.context());
    } catch (error) {
      this.message = `无法打开存储目录：${(error as Error).message}`;
    }
  }

  private async clean(mode: string): Promise<void> {
    this.busy = true;
    AppStorage.setOrCreate('storageBusy', true);
    this.message = '';
    try {
      if (mode === 'all') {
        // 由系统清空真实沙箱和配置并退出进程，不把递归删除权限扩到外部目录。
        await this.context().getApplicationContext().clearUpApplicationData();
      } else {
        const count = await cleanWorkspaceCache(this.context(), mode);
        this.message = `已清理 ${count} 个${mode === 'inputs' ? '输入缓存目录' : '重复工作区'}`;
        this.context().getApplicationContext().eventHub.emit('huiwen.settings.changed');
      }
    } catch (error) {
      this.message = (error as Error).message;
    } finally {
      this.busy = false;
      AppStorage.setOrCreate('storageBusy', false);
    }
  }

  private async changeScreenshotLocation(cache: boolean, restore: boolean): Promise<void> {
    this.busy = true;
    AppStorage.setOrCreate('storageBusy', true);
    this.message = '';
    try {
      if (cache) { await closeScreenshotPreview(); }
      if (await changeScreenshotDirectory(this.context(), cache, restore)) {
        this.message = cache ? '截图缓存目录已更新' : '截图下载目录已创建并可用';
      }
    } catch (error) {
      this.message = (error as Error).message;
    } finally {
      this.reload();
      this.busy = false;
      AppStorage.setOrCreate('storageBusy', false);
    }
  }

  private async browseScreenshotLocation(cache: boolean): Promise<void> {
    this.busy = true;
    AppStorage.setOrCreate('storageBusy', true);
    this.message = '';
    try {
      await openScreenshotDirectory(this.context(), cache);
    } catch (error) {
      this.message = `无法打开截图目录：${(error as Error).message}`;
    } finally {
      this.reload();
      this.busy = false;
      AppStorage.setOrCreate('storageBusy', false);
    }
  }

  private confirmClean(mode: string): void {
    const all = mode === 'all';
    this.getUIContext().showAlertDialog({
      title: all ? '清理全部应用数据？' : mode === 'duplicates' ? '清理重复工作区？' : '清理默认输入缓存？',
      message: all ? '将删除应用沙箱中的全部文档、缓存和设置，撤销已申请的用户权限，并退出应用。自定义目录中的工作区不删除。此操作无法撤销。' :
        mode === 'duplicates' ? '按仍存在的输入文件名和模式分组，仅保留最近使用的一份，其余工作区连同输出一起删除。同名文件可能内容不同，此操作无法撤销。' :
          '仅删除默认沙箱中所有 input 目录，保留 output 和预览文件。对应工作区将无法再次恢复输入文档，此操作无法撤销。',
      primaryButton: { value: '取消', action: () => {} },
      secondaryButton: { value: '确认清理', action: () => { this.clean(mode); } },
      autoCancel: false
    });
  }

  @Builder
  sectionTitle(title: string, colorHelp: boolean = false) {
    Column({ space: 8 }) {
      Row({ space: 8 }) {
        Text(title)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
        if (colorHelp) {
          Button({ type: ButtonType.Normal }) {
            Image($r('app.media.titlebar_color_help')).width(18).height(18).draggable(false)
          }
          .width(24)
          .height(24)
          .padding(3)
          .backgroundColor(Color.Transparent)
          .borderRadius(12)
          .accessibilityText('查看透明边框文字颜色说明')
          .onHover((hovered: boolean): void => { this.colorHelpVisible = hovered; })
          .onClick((): void => { this.colorHelpVisible = true; })
          .bindPopup(this.colorHelpVisible, {
            message: TITLE_COLOR_HELP,
            placement: Placement.Bottom,
            mask: false,
            onStateChange: (event): void => { this.colorHelpVisible = event.isVisible; }
          })
        }
      }
      Divider()
        .width('80%')
        .strokeWidth(1)
        .color($r('app.color.home_border'))
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
    .margin({ bottom: 8 })
  }

  @Builder
  choice(label: string, group: string, selected: boolean, action: () => void, repeatable: boolean = false) {
    Row({ space: 8 }) {
      Radio({ value: label, group: group })
        .checked(selected)
        .width(20)
        .height(20)
        .radioStyle({ checkedBackgroundColor: '#3983F7' })
        .onChange((checked: boolean) => {
          if (checked && !selected) { action(); }
        })
        // 自动模式可能已选中但尚未授权，允许再次点击来申请权限。
        .onClick(() => { if (selected && repeatable) { action(); } })
      Text(label)
        .fontSize(14)
        .fontColor($r('app.color.home_text'))
        .layoutWeight(1)
        .onClick(() => { if (!selected || repeatable) { action(); } })
    }
    .width('100%')
    .constraintSize({ minHeight: 36 })
    .enabled(!this.busy)
  }

  @Builder
  actionButton(label: string, action: () => void, destructive: boolean = false) {
    Button(label, { type: ButtonType.Normal })
      .height(34)
      .fontSize(14)
      .fontColor(destructive ? $r('app.color.home_warning') : $r('app.color.home_text'))
      .backgroundColor($r('app.color.home_menu_hover'))
      .borderRadius(8)
      .padding({ left: 14, right: 14 })
      .enabled(!this.busy)
      .onClick(() => action())
  }

  @Builder
  cleanupRow(title: string, description: string, mode: string) {
    Row({ space: 16 }) {
      Column({ space: 4 }) {
        Text(title)
          .fontSize(14)
          .fontColor($r('app.color.home_text'))
        Text(description)
          .fontSize(12)
          .fontColor($r('app.color.home_muted'))
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start)
      this.actionButton(mode === 'all' ? '清理全部' : '清理', () => this.confirmClean(mode), mode === 'all')
    }
    .width('100%')
    .padding({ top: 8, bottom: 8 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder
  screenshotDirectory(title: string, path: string, cache: boolean) {
    Column({ space: 8 }) {
      Text(title).fontSize(14).fontColor($r('app.color.home_text'))
      Row({ space: 10 }) {
        Text(path)
          .fontSize(13)
          .fontFamily('monospace')
          .fontColor($r('app.color.home_text'))
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .ellipsisMode(EllipsisMode.CENTER)
          .copyOption(CopyOptions.InApp)
          .height(38)
          .layoutWeight(1)
          .padding({ left: 12, right: 12 })
          .backgroundColor($r('app.color.home_background'))
          .border({ width: 1, color: $r('app.color.home_border') })
          .borderRadius(8)
        this.actionButton('浏览', () => this.browseScreenshotLocation(cache))
      }
      .width('100%')
      Row({ space: 12 }) {
        this.actionButton('修改位置', () => this.changeScreenshotLocation(cache, false))
        this.actionButton('恢复默认路径', () => this.changeScreenshotLocation(cache, true))
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
  }

  build() {
    HomeDialog({ title: '应用设置', titleIcon: $r('app.media.menu_settings'), dialogWidth: this.dialogWidth,
      onCloseRequested: () => { if (!this.busy) { this.onCloseRequested(); } }
    }) {
      Scroll() {
        Column({ space: 24 }) {
          Column({ space: 10 }) {
            this.sectionTitle('默认存储文件路径')
            Row({ space: 10 }) {
              Text(this.storagePath)
                .fontSize(13)
                .fontFamily('monospace')
                .fontColor($r('app.color.home_text'))
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
                .ellipsisMode(EllipsisMode.CENTER)
                .copyOption(CopyOptions.InApp)
                .height(38)
                .layoutWeight(1)
                .padding({ left: 12, right: 12 })
                .backgroundColor($r('app.color.home_background'))
                .border({ width: 1, color: $r('app.color.home_border') })
                .borderRadius(8)
              this.actionButton('浏览', () => { this.browseLocation(); })
            }
            .width('100%')
            Row({ space: 12 }) {
              this.actionButton('修改位置', () => { this.changeLocation(false); })
              this.actionButton('恢复默认路径', () => { this.changeLocation(true); })
            }
            .width('100%')
          }
          .width('100%')
          .alignItems(HorizontalAlign.Start)
          Column({ space: 4 }) {
            this.sectionTitle('最近工作区条数')
            ForEach([3, 5, 10], (count: number) => {
              this.choice(String(count), 'recent-limit', this.recentLimit === count,
                () => { this.saveChoice('recentLimit', String(count)); })
            }, (count: number) => String(count))
          }
          .width('100%')
          Column({ space: 4 }) {
            this.sectionTitle('工作区关闭按钮设置')
            this.choice('回到首页', 'close-action', this.closeAction === 'home',
              () => { this.saveChoice('closeAction', 'home'); })
            this.choice('退出应用', 'close-action', this.closeAction === 'exit',
              () => { this.saveChoice('closeAction', 'exit'); })
          }
          .width('100%')
          Column({ space: 4 }) {
            this.sectionTitle('清理缓存')
            this.cleanupRow('输入文件缓存', '仅清理应用沙箱中的 input，保留输出文件。', 'inputs')
            this.cleanupRow('重复工作区', '同名、同模式保留最近使用的一份。', 'duplicates')
            this.cleanupRow('全部应用数据', '删除沙箱文档、缓存与配置，并退出应用。', 'all')
          }
          .width('100%')
          .alignItems(HorizontalAlign.Start)
          Column({ space: 4 }) {
            this.sectionTitle('透明边框文字颜色', true)
            Row({ space: 12 }) {
              ForEach([TitleTextColor.Follow, TitleTextColor.Black, TitleTextColor.White], (mode: TitleTextColor) => {
                Column() {
                  this.choice(mode === TitleTextColor.Follow ? '跟随壁纸' :
                    mode === TitleTextColor.Black ? '黑色' : '白色', 'title-text-color', this.titleTextColor === mode,
                    () => { this.saveTitleTextColor(mode); }, mode === TitleTextColor.Follow)
                }
                .layoutWeight(1)
              }, (mode: TitleTextColor): string => mode)
            }
            .width('100%')
          }
          .width('100%')
          Column({ space: 12 }) {
            this.sectionTitle('截图设置')
            this.screenshotDirectory('截图下载目录', this.screenshotDownloadPath, false)
            this.screenshotDirectory('截图缓存目录', this.screenshotCachePath, true)
            Row({ space: 12 }) {
              Text('隐藏本应用截图').fontSize(14).fontColor($r('app.color.home_text')).width(140)
              ShortcutInput({ controlId: 'hidden-screenshot-shortcut', shortcut: this.hiddenShortcut,
                available: !this.busy, onChange: (value: ScreenshotShortcut) => { this.saveShortcut(true, value); } })
            }
            .width('100%')
            Row({ space: 12 }) {
              Text('截取当前屏幕').fontSize(14).fontColor($r('app.color.home_text')).width(140)
              ShortcutInput({ controlId: 'screen-screenshot-shortcut', shortcut: this.screenShortcut,
                available: !this.busy, onChange: (value: ScreenshotShortcut) => { this.saveShortcut(false, value); } })
            }
            .width('100%')
            Text('点击键帽修改，Esc 取消。工作区打开时暂不支持截图快捷键。')
              .fontSize(12)
              .fontColor($r('app.color.home_muted'))
          }
          .width('100%')
          if (this.message !== '') {
            Text(this.message)
              .fontSize(13)
              .fontColor($r('app.color.home_warning'))
              .width('100%')
          }
        }
        .width('100%')
        .padding({ right: 12 })
        .alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      .height(this.contentHeight)
      .scrollBar(BarState.Auto)
      .align(Alignment.TopStart)
      // 按下时取消旧录入；键帽随后在点击时开始新录入，无需阻断点击冒泡。
      .onTouch((event: TouchEvent) => {
        if (event.type === TouchType.Down) {
          this.context().getApplicationContext().eventHub.emit(CANCEL_SHORTCUT_RECORDING);
        }
      })
      .onMouse((event: MouseEvent) => {
        if (event.action === MouseAction.Press) {
          this.context().getApplicationContext().eventHub.emit(CANCEL_SHORTCUT_RECORDING);
        }
      })
    }
  }
}
