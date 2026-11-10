#pragma once
#include <QTimer>
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
    void aboutDeviceRequested();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void updateMetrics();
    void refreshHover();
    void updateWindowState();
    QWidget *window_;
    QLabel *title_;
    QToolButton *harmonyLogo_;
    QToolButton *maximize_ = nullptr;
    QToolButton *minimize_ = nullptr;
    QToolButton *close_ = nullptr;
    QTimer hoverTimer_;
    qreal scale_ = 1;
#ifdef Q_OS_OPENHARMONY
    qint32 displayId_ = -1;
#endif
};
