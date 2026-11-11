/* 导出确认弹窗：展示目标文件夹名称，确认后再由工作区打开系统目录选择器。 */
#pragma once
#include <QDialog>

class ExportOutputDialog : public QDialog
{
  public:
    explicit ExportOutputDialog(const QString &folderName, QWidget *parent = nullptr);
};
