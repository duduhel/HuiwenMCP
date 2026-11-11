// 工作区输出导出：选择系统目录，将 MD 与同目录资源复制到独立文件夹，不覆盖已有结果。
import { common } from '@kit.AbilityKit';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import nativeBridge from 'libhuiwen_native.so';
import { ensureWorkspaceAccess } from './settings/WorkspaceStorage';

// 保留 output 内的相对目录结构；临时文件不导出，符号链接不跨出已校验的输出目录。
async function copyOutputDirectory(source: string, destination: string): Promise<void> {
  const names = await fileIo.listFile(source);
  for (const name of names) {
    if (name.startsWith('.pdf2md-')) { continue; }
    const input = `${source}/${name}`;
    const output = `${destination}/${name}`;
    const info = await fileIo.lstat(input);
    if (info.isSymbolicLink()) { throw new Error('输出资源包含符号链接，无法导出'); }
    if (info.isDirectory()) {
      await fileIo.mkdir(output);
      await copyOutputDirectory(input, output);
    } else if (info.isFile()) {
      await fileIo.copyFile(input, output);
    } else {
      throw new Error('输出资源包含不支持的文件类型');
    }
  }
}

export async function exportWorkspaceOutput(context: common.UIAbilityContext, path: string): Promise<string> {
  const root = await ensureWorkspaceAccess(context);
  const output = nativeBridge.sandboxValidateOutputFile(root, path);
  const source = nativeBridge.sandboxValidateTaskDirectory(root, output.slice(0, output.lastIndexOf('/')));
  const options = new picker.DocumentSelectOptions();
  options.selectMode = picker.DocumentSelectMode.FOLDER;
  options.maxSelectNumber = 1;
  options.defaultFilePathUri = 'file://docs/storage/Users/currentUser/Documents';
  const selected = await new picker.DocumentViewPicker(context).select(options);
  if (selected.length === 0) { return ''; }
  const selectedPath = new fileUri.FileUri(selected[0]).path;
  const directory = selectedPath.endsWith('/') ? selectedPath.slice(0, -1) : selectedPath;
  if (directory === source || directory.startsWith(`${source}/`)) {
    throw new Error('导出目录不能位于当前 output 内');
  }
  // 选择器返回后重新校验，避免选择期间原工作区文件已失效。
  const currentRoot = await ensureWorkspaceAccess(context);
  nativeBridge.sandboxValidateOutputFile(currentRoot, output);
  nativeBridge.sandboxValidateTaskDirectory(currentRoot, source);
  const filename = output.slice(output.lastIndexOf('/') + 1);
  const name = filename.slice(0, filename.lastIndexOf('.')).slice(0, 64);
  let destination = `${directory}/${name}`;
  let suffix = 1;
  while (await fileIo.access(destination)) {
    destination = `${directory}/${name} (${suffix++})`;
  }
  await fileIo.mkdir(destination);
  try {
    await copyOutputDirectory(source, destination);
  } catch (error) {
    // 只删除本次创建的副本，保留沙箱源文件与目标目录原有文件。
    try {
      await fileIo.rmdir(destination);
    } catch (cleanupError) {
      hilog.warn(0xFF00, 'WorkspaceExport', 'HUIWEN_QT_EXPORT_CLEANUP_FAILED: %{public}s',
        (cleanupError as Error).message);
    }
    throw new Error((error as Error).message);
  }
  return destination;
}
