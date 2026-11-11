// 工作区输出导出：选择单个文件保存，或将 MD 与资源整体复制到独立文件夹。
import { common } from '@kit.AbilityKit';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import nativeBridge from 'libhuiwen_native.so';
import { ensureWorkspaceAccess } from './settings/WorkspaceStorage';

const DEFAULT_EXPORT_URI: string = 'file://docs/storage/Users/currentUser/Documents';

// 单文件必须是 output 内的真实文件；逐层拒绝路径穿越、临时目录和符号链接。
async function selectedOutputFile(source: string, relativePath: string): Promise<string> {
  if (relativePath.includes('\\') || relativePath.includes('\0')) {
    throw new Error('导出文件路径无效');
  }
  const parts = relativePath.split('/');
  let path = source;
  for (let index = 0; index < parts.length; index++) {
    const name = parts[index];
    if (name === '' || name === '.' || name === '..' || name.startsWith('.pdf2md-')) {
      throw new Error('所选文件不属于可导出的 output 内容');
    }
    path = `${path}/${name}`;
    const info = await fileIo.lstat(path);
    if (info.isSymbolicLink() ||
      (index === parts.length - 1 ? !info.isFile() : !info.isDirectory())) {
      throw new Error('所选输出文件已失效或包含符号链接');
    }
  }
  return path;
}

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

export async function exportWorkspaceOutput(context: common.UIAbilityContext, path: string,
  relativePath: string = ''): Promise<string> {
  const root = await ensureWorkspaceAccess(context);
  const output = nativeBridge.sandboxValidateOutputFile(root, path);
  const source = nativeBridge.sandboxValidateTaskDirectory(root, output.slice(0, output.lastIndexOf('/')));
  if (relativePath !== '') {
    const file = await selectedOutputFile(source, relativePath);
    const saveOptions = new picker.DocumentSaveOptions();
    saveOptions.newFileNames = [file.slice(file.lastIndexOf('/') + 1)];
    saveOptions.defaultFilePathUri = DEFAULT_EXPORT_URI;
    const saved = await new picker.DocumentViewPicker(context).save(saveOptions);
    if (saved.length === 0) { return ''; }
    const destination = new fileUri.FileUri(saved[0]).path;
    if (destination === root || destination.startsWith(`${root}/`)) {
      throw new Error('请选择工作区之外的保存位置，避免覆盖输入输出缓存');
    }
    const currentRoot = await ensureWorkspaceAccess(context);
    nativeBridge.sandboxValidateOutputFile(currentRoot, output);
    nativeBridge.sandboxValidateTaskDirectory(currentRoot, source);
    const input = await fileIo.open(await selectedOutputFile(source, relativePath), fileIo.OpenMode.READ_ONLY);
    try {
      const target = await fileIo.open(saved[0],
        fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.CREATE | fileIo.OpenMode.TRUNC);
      try {
        await fileIo.copyFile(input.fd, target.fd);
      } finally {
        await fileIo.close(target);
      }
    } finally {
      await fileIo.close(input);
    }
    return destination;
  }
  const options = new picker.DocumentSelectOptions();
  options.selectMode = picker.DocumentSelectMode.FOLDER;
  options.maxSelectNumber = 1;
  options.defaultFilePathUri = DEFAULT_EXPORT_URI;
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
