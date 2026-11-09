/* 工作区顶部功能条：组装文件与滚动菜单，不依赖预览或转换服务。 */
#pragma once
#include <QToolBar>
class FileMenu;
class QAction;
class QToolButton;

class FunctionBar : public QToolBar
{
    Q_OBJECT
  public:
    explicit FunctionBar(const QString &mode, QWidget *parent = nullptr);
    FileMenu *fileMenu() const;
    bool isScrollSynchronized() const;
    QAction *addCommand(const QString &text);
    void setNavigationSidebarVisible(bool visible);

  signals:
    void scrollSyncChanged(bool synchronized);
    void navigationSidebarToggled(bool visible);

  private:
    FileMenu *fileMenu_;
    QAction *synchronizedAction_ = nullptr;
    QAction *trailingSpacer_ = nullptr;
    QToolButton *navigationToggle_ = nullptr;
};
