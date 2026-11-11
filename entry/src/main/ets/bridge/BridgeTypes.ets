// Qt／ArkTS 会话协议；JSON 和 action 只在桥接层使用，业务代码使用具名方法。
import { HarmonySandboxDocument } from 'libhuiwen_native.so';

// Qt → ArkTS：TSFN 投递的事件；sessionId 标记发送时的会话，其他字段按 action 使用。
export interface BridgeEvent {
  sessionId: number;
  action: string;
  path?: string;
  relativePath?: string;
  kind?: string;
  mode?: string;
  taskPath?: string;
  error?: string;
  format?: string;
  value?: string;
  color?: string;
  displayId?: number;
  ok?: boolean;
  cached?: boolean;
}

// ArkTS → Qt：具名方法组装的结果；document 为空表示取消或失败，error 为空表示无错误。
export interface BridgeResult {
  action: string;
  document?: HarmonySandboxDocument;
  path?: string;
  error?: string;
}

// 工作区 Ability 提供的业务处理器，在 ArkTS 线程执行；Promise 只等待本次处理，不代表整个窗口生命周期。
export interface WorkspaceHandlers {
  // Qt 窗口已曝光：通知首页结束，再按需恢复任务。
  onReady: () => Promise<void>;
  // Qt 关闭按钮请求：由 Ability 根据设置返回首页或退出。
  onReturnHome: () => Promise<void>;
  // 编辑关闭确认结果：approved 为 true 才继续关闭，false 取消本次请求。
  onCloseApproval: (approved: boolean) => Promise<void>;
  // Qt 已隐藏窗口并完成 CEF 释放：Ability 才执行最终结束操作。
  onClosed: () => Promise<void>;
  // 任务恢复回执：taskPath 用于匹配待恢复任务，error 为空表示成功。
  onRestoreResult: (taskPath: string, error: string) => void;
  // 系统选取请求：kind 为文档格式，mode 为 preview／edit／convert。
  onSelectDocument: (kind: string, mode: string) => Promise<void>;
  // 定位已生成文件：path 是待校验的输出文件路径。
  onOpenOutputLocation: (path: string) => Promise<void>;
  // 打开任务目录：path 是待校验的任务或 input／output 目录。
  onOpenTaskDirectory: (path: string) => Promise<void>;
  // 导出输出：path 为待校验的 MD 锚点，relativePath 为空导出整个 output，否则只导出该文件。
  onExportOutput: (path: string, relativePath: string) => Promise<void>;
  // 鸿蒙字标点击：打开系统设置的“关于本机”。
  onOpenAboutDevice: () => Promise<void>;
}

// 截图 Ability 提供的业务处理器，在 ArkTS 线程完成历史入库、目录授权和全局提示。
export interface ScreenshotHandlers {
  // 保存请求：path 为缓存图片，error 为 Qt 保存错误，displayId 用于定位提示屏幕。
  onDownload: (path: string, error: string, displayId: number) => Promise<void>;
  // 颜色已复制：format 为 HEX／RGB，value 为剪贴板文本，color 为历史色块的 HEX 值。
  onColorCopied: (format: string, value: string, color: string, displayId: number) => Promise<void>;
  // 截图结束：ok 为是否成功，path 为结果，cached 表示是否已缓存；取消不新增历史。
  onFinished: (ok: boolean, path: string, error: string, cached: boolean) => Promise<void>;
}
