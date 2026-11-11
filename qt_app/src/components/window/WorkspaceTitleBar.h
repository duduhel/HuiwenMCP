#pragma once
#include <QTimer>
#include <QWidget>
class QLabel;
class QMenu;
class QToolButton;
class TitleBarActions;

// 标题栏直接组合 TitleBarActions 与窗口三键；拖动与双击复用 cust。
class WorkspaceTitleBar : public QWidget
{
    Q_OBJECT

  public:
    WorkspaceTitleBar(const QString &mode, QWidget *window);
    TitleBarActions *titleActions() const;
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
    TitleBarActions *titleActions_;
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
