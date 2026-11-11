/* 导出确认弹窗：单文件直接展示，多文件默认导出整个文件夹，也可选择其中一个文件。 */
#pragma once
#include <QDialog>
class QComboBox;

class ExportOutputDialog : public QDialog
{
  public:
    explicit ExportOutputDialog(const QString &outputPath, QWidget *parent = nullptr);
    // 相对 output 的文件路径；空字符串表示整个文件夹。
    QString selectedRelativePath() const;

  private:
    QComboBox *selection_ = nullptr;
    QString onlyFile_;
};
