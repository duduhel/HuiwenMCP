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
    // 业务控件插入弹性留白之前，右侧导出按钮保持固定。
    void addLeadingWidget(QWidget *widget);
    void setExportEnabled(bool enabled);

  signals:
    void navigationSidebarToggled(bool visible);
    void exportRequested();

  protected:
    bool event(QEvent *event) override;

  private:
    void updateAppearance();
    QToolButton *navigationToggle_ = nullptr;
    QToolButton *exportButton_ = nullptr;
    QAction *stretchAction_ = nullptr;
    QWidget *leadingSpace_ = nullptr;
    QWidget *trailingSpace_ = nullptr;
};
