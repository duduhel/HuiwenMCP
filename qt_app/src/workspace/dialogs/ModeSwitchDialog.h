/* 切换操作模式前，只从当前任务的 input／output 中选择一个可用文件。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include <QDialog>
class QRadioButton;

class ModeSwitchDialog : public QDialog
{
  public:
    ModeSwitchDialog(const DocumentItem &document, const QString &mode, QWidget *parent = nullptr);
    QString selectedPath() const;
    static bool hasUsableFile(const DocumentItem &document, const QString &mode);

  private:
    static QString usablePath(const DocumentItem &document, const QString &mode, bool output,
                              QString &reason);
    QString inputPath_;
    QString outputPath_;
    QRadioButton *input_;
    QRadioButton *output_;
};
