/* 独立业务模式菜单：只维护可用状态并发出请求，不负责重建文档视图。 */
#pragma once
#include "SelectionMenu.h"
#include "sandbox/DocumentItem.h"
#include <QVector>
class QAction;

class ModeMenu : public SelectionMenu
{
    Q_OBJECT
  public:
    explicit ModeMenu(QWidget *parent = nullptr);
    void setState(const QString &mode, const DocumentItem &document, bool busy);
  signals:
    void modeRequested(const QString &mode);
    void replacementRequested();

  private:
    QString mode_;
    QVector<QAction *> modes_;
    SelectionMenu *replacementMenu_;
    QAction *replacementAction_;
};
