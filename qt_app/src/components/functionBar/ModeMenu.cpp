/* 蓝色选中点沿用 SelectionMenu；只有当前模式的子菜单提供更换文件入口。 */
#include "ModeMenu.h"
#include "workspace/WorkspaceMode.h"
#include "workspace/dialogs/ModeSwitchDialog.h"
#include <QAction>
#include <QVariant>

ModeMenu::ModeMenu(QWidget *parent)
    : SelectionMenu(parent), replacementMenu_(new SelectionMenu(this))
{
    for (const QString &mode : {QString("preview"), QString("edit"), QString("convert")})
    {
        auto *action = addAction(WorkspaceMode::title(mode));
        action->setData(mode);
        action->setCheckable(true);
        modes_.append(action);
        connect(action, &QAction::triggered, this,
                [this, mode]
                {
                    // 选择只发请求；弹窗取消或加载失败时仍保持原模式的选中点。
                    for (auto *item : modes_)
                        item->setChecked(item->data().toString() == mode_);
                    emit modeRequested(mode);
                });
    }
    replacementAction_ = replacementMenu_->addAction(QStringLiteral("更换文件"));
    connect(replacementAction_, &QAction::triggered, this, &ModeMenu::replacementRequested);
}

void ModeMenu::setState(const QString &mode, const DocumentItem &document, bool busy)
{
    mode_ = mode;
    replacementMenu_->setTitle(WorkspaceMode::title(mode));
    for (auto *action : modes_)
        if (action->menu() && action->data().toString() != mode)
            action->setMenu(nullptr);
    for (auto *action : modes_)
    {
        const QString target = action->data().toString();
        const bool current = target == mode;
        action->setChecked(current);
        action->setMenu(current ? replacementMenu_ : nullptr);
        const bool available = current || ModeSwitchDialog::hasUsableFile(document, target);
        action->setEnabled(!busy && available);
        action->setToolTip(available ? QString()
                                     : QStringLiteral("当前工作区没有支持此模式的文件"));
    }
    replacementAction_->setEnabled(!busy);
}
