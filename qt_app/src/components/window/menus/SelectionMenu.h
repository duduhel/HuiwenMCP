/* 共用菜单样式：右侧蓝色圆点表示选中项，子菜单箭头单独留在最右侧。 */
#pragma once
#include <QMenu>

class SelectionMenu : public QMenu
{
  public:
    explicit SelectionMenu(QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *event) override;
};
