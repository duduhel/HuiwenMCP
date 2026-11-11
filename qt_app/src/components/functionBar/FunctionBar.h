/* 工作区功能条：保留侧栏入口，菜单控件可由标题栏承载，业务信号保持统一。 */
#pragma once
#include <QToolBar>
class FileMenu;
class ModeMenu;
class QAction;
class QToolButton;

class FunctionBar : public QToolBar
{
    Q_OBJECT
  public:
    explicit FunctionBar(const QString &mode, QWidget *parent = nullptr);
    QToolBar *commandBar() const;
    FileMenu *fileMenu() const;
    ModeMenu *modeMenu() const;
    void setMode(const QString &mode);
    bool isScrollSynchronized() const;
    QAction *addCommand(const QString &text);
    void setNavigationSidebarVisible(bool visible);

  signals:
    void scrollSyncChanged(bool synchronized);
    void navigationSidebarToggled(bool visible);

  protected:
    bool event(QEvent *event) override;

  private:
    void updateAppearance();
    QToolBar *commands_;
    FileMenu *fileMenu_;
    ModeMenu *modeMenu_;
    QAction *synchronizedAction_ = nullptr;
    QToolButton *navigationToggle_ = nullptr;
    QToolButton *scrollButton_ = nullptr;
};
