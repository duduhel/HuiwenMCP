/* 工作区功能条：管理侧栏开关及后续业务入口，与标题栏菜单相互独立。 */
#pragma once
#include <QToolBar>
class QToolButton;

class FunctionBar : public QToolBar
{
    Q_OBJECT
  public:
    explicit FunctionBar(QWidget *parent = nullptr);
    void setNavigationSidebarVisible(bool visible);

  signals:
    void navigationSidebarToggled(bool visible);

  protected:
    bool event(QEvent *event) override;

  private:
    void updateAppearance();
    QToolButton *navigationToggle_ = nullptr;
};
