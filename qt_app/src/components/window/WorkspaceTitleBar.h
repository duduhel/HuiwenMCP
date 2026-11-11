#pragma once
#include <QTimer>
#include <QWidget>
class QLabel;
class QMenu;
class QToolButton;
class QToolBar;

// 标题栏承载菜单与窗口操作；菜单业务留在 FunctionBar，拖动与双击复用 cust。
class WorkspaceTitleBar : public QWidget
{
    Q_OBJECT

  public:
    WorkspaceTitleBar(QWidget *window, QToolBar *commands);
  signals:
    void closeRequested();
    void aboutDeviceRequested();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void updateMetrics();
    void refreshHover();
    void updateWindowState();
    void showMaximizeMenu();
    void hideMaximizeMenu();
    QWidget *window_;
    QLabel *title_;
    QToolButton *harmonyLogo_;
    QToolButton *maximize_ = nullptr;
    QToolButton *minimize_ = nullptr;
    QToolButton *close_ = nullptr;
    QTimer hoverTimer_;
    QTimer maximizeMenuDelay_;
    QMenu *maximizeMenu_ = nullptr;
    qreal scale_ = 1;
#ifdef Q_OS_OPENHARMONY
    qint32 displayId_ = -1;
#endif
};
