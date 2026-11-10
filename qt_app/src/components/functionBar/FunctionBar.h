/* 工作区顶部功能条：组装文件、滚动设置与模式菜单，不依赖预览或转换服务。 */
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
    FileMenu *fileMenu_;
    ModeMenu *modeMenu_;
    QAction *synchronizedAction_ = nullptr;
    QAction *trailingSpacer_ = nullptr;
    QToolButton *navigationToggle_ = nullptr;
    QToolButton *scrollButton_ = nullptr;
};
