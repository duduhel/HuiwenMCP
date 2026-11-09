// ArkTS 调用 libhuiwen_native.so 时使用的类型声明。
import { resourceManager } from '@kit.LocalizationKit';
export interface HarmonySandboxDocument {
  name: string;
  kind: string;
  mode: string;
  taskPath: string;
  inputPath: string;
  outputPath: string;
  openPath: string;
  lastUsedAt: number;
}

// 文件管理记录：只返回真实存在且可读的输入／输出文件。
export interface HarmonySandboxFile {
  name: string;
  path: string;
  role: string;
  kind: string;
  taskPath: string;
  workspaceName: string;
  mode: string;
  sourceKind: string;
  lastUsedAt: number;
  sizeBytes: number;
}

interface HuiwenNative {
  // ArkTS → Native，同步返回 NAPI OK，仅检查模块是否能正常调用。
  ping(): string;
  // ArkTS → Native，同步从资源部署内置转换 pyz 到 cacheDir，不接收外部脚本。
  configurePython(resources: resourceManager.ResourceManager, cacheDir: string): void;
  // ArkTS → Native，工作线程执行固定 Python 自检；Promise 返回自检文本，失败拒绝。
  checkPython(): Promise<string>;
  // ArkTS → Native，工作线程调用 Python 将沙箱 PDF 写成 MD；Promise 返回输出文件路径，失败拒绝。
  convertPdf(inputPath: string, outputPath: string): Promise<string>;
  // ArkTS → Native，后台抓屏并返回 Promise<void>；兑现后才启动截图宿主，Qt 取走底图接管释放。
  screenshotPrepareCapture(): Promise<void>;
  // ArkTS → Native，同步释放尚未交给 Qt 的底图；用于截图宿主启动失败或销毁，返回 void。
  screenshotReleaseCapture(): void;
  // ArkTS → Native：授权 fd 同步复制后后台导入，Promise 返回沙箱文档；调用方关闭原 fd。
  sandboxImportDocument(filesDir: string, name: string, fd: number, kind: string,
    mode: string): Promise<HarmonySandboxDocument>;
  // ArkTS → Native：同步校验 filesDir 内输出文件并返回有效路径，无效路径抛异常。
  sandboxValidateOutputFile(filesDir: string, path: string): string;
  // ArkTS → Native，同步校验并返回任务或 input/output 目录；转换前也能打开空输出目录。
  sandboxValidateTaskDirectory(filesDir: string, path: string): string;
  // ArkTS → Native，Promise 返回最近任务数组；mode 为 all/preview/edit/convert，offset 默认 0。
  sandboxListRecentWorkspaces(filesDir: string, mode: string, limit: number,
    offset?: number): Promise<HarmonySandboxDocument[]>;
  // ArkTS → Native，后台校验 taskPath 并返回 Promise<沙箱文档>；Qt 确认载入后才更新时间。
  sandboxLoadWorkspace(filesDir: string, taskPath: string): Promise<HarmonySandboxDocument>;
  // ArkTS → Native，Promise 返回可读文件数组，按工作区最近使用时间降序；查询不更新时间。
  sandboxListFiles(filesDir: string): Promise<HarmonySandboxFile[]>;
  // ArkTS → Native，后台复制并校验工作区；Promise 返回真实目标根目录，保留源文件，不覆盖已有任务。
  sandboxCopyWorkspaces(sourceRoot: string, targetRoot: string): Promise<string>;
  // ArkTS → Native，Promise 返回清理数量；inputs 只清理 input，duplicates 清理同名同模式旧任务。
  // workspaces 清理迁移前副本；copiedRoot 供清理前核对已复制的数据。
  sandboxCleanWorkspaces(root: string, mode: string, copiedRoot?: string): Promise<number>;
  // ArkTS → Native：同步注册 callback／mode／kind 并返回会话 ID，替换前一会话的回调。
  // callback 由 TSFN 投递到 ArkTS 线程；截图使用 screenshot、空 kind 和已授权 screenshotCacheRoot。
  sessionAttach(callback: (event: string) => void, mode: string, kind: string, screenshotCacheRoot?: string): number;
  // ArkTS → Native：同步注销指定 ID；旧 ID 不影响当前会话，返回 void。
  sessionDetach(sessionId: number): void;
  // ArkTS → Native：发送该 ID 的结果 JSON，Native 校验后排队投递 Qt；返回 void 不代表业务完成。
  sessionDeliverResult(sessionId: number, result: string): void;
}

declare const nativeBridge: HuiwenNative;
export default nativeBridge;
