#pragma once
#include <QWidget>
class QLabel;
class QToolButton;

// 标题栏只负责窗口操作，拖动与双击由 cust 的 QObject 辅助类处理。
class WorkspaceTitleBar : public QWidget
{
    Q_OBJECT

  public:
    explicit WorkspaceTitleBar(QWidget *window);
  signals:
    void closeRequested();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void updateWindowState();
    QWidget *window_;
    QLabel *title_;
    QToolButton *maximize_;
};
