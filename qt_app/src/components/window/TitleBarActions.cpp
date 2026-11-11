/* 标题栏功能菜单 */
#include "TitleBarActions.h"
#include "menus/FileMenu.h"
#include "menus/ModeMenu.h"
#include "menus/SelectionMenu.h"
#include <QAction>
#include <QActionGroup>
#include <QFontMetrics>
#include <QLayout>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <algorithm>

TitleBarActions::TitleBarActions(const QString &mode, QWidget *parent)
    : QToolBar(parent), fileMenu_(new FileMenu(mode, this)), modeMenu_(new ModeMenu(this))
{
    setObjectName(QStringLiteral("workspaceTitleActions"));
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumWidth(0);
    setStyleSheet(QStringLiteral(
        "QToolBar#workspaceTitleActions { background: transparent; border: none; "
        "margin: 0; padding: 0; spacing: 0; }"
        "QToolBar#workspaceTitleActions QToolButton { background: transparent; border: none; "
        "border-radius: 8px; margin: 0; padding: 0 20px; color: #000000; "
        "font-size: 26px; font-weight: 550; }"
        "QToolBar#workspaceTitleActions QToolButton[selected=\"true\"] "
        "{ background: #DCE1E8; }"
        "QToolBar#workspaceTitleActions QToolButton:disabled { color: #A0A8B4; }"
        "QToolBar#workspaceTitleActions QToolButton::menu-indicator "
        "{ image: none; width: 0; height: 0; }"));
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(0);

    const auto addMenuButton = [this](const QString &text, QMenu *menu)
    {
        auto *button = new QToolButton(this);
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setCursor(Qt::PointingHandCursor);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setMenu(menu);
        button->setProperty("selected", false);
        // 背景只取决于菜单是否展开，避免关闭后残留 hover/pressed 高亮。
        const auto setSelected = [button](bool selected)
        {
            button->setProperty("selected", selected);
            button->style()->unpolish(button);
            button->style()->polish(button);
            button->update();
        };
        connect(menu, &QMenu::aboutToShow, button, [setSelected] { setSelected(true); });
        connect(menu, &QMenu::aboutToHide, button, [setSelected] { setSelected(false); });
        // 先应用 26px 字体再测量宽度，避免标题栏文字裁切。
        button->ensurePolished();
        const int textWidth = std::max(button->fontMetrics().horizontalAdvance(text),
                                       button->fontMetrics().boundingRect(text).width());
        button->setFixedWidth(std::max(button->sizeHint().width(), textWidth + 40));
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        addWidget(button);
        return button;
    };
    addMenuButton(QStringLiteral("文件"), fileMenu_);
    auto *menu = new SelectionMenu(this);
    auto *modes = new QActionGroup(menu);
    modes->setExclusive(true);
    synchronizedAction_ = menu->addAction(QStringLiteral("同步滚动"));
    auto *oneWay = menu->addAction(QStringLiteral("单向滚动"));
    for (auto *action : {synchronizedAction_, oneWay})
    {
        action->setCheckable(true);
        modes->addAction(action);
    }
    synchronizedAction_->setChecked(true);
    connect(synchronizedAction_, &QAction::toggled, this, &TitleBarActions::scrollSyncChanged);
    scrollButton_ = addMenuButton(QStringLiteral("滚动设置"), menu);
    addMenuButton(QStringLiteral("模式"), modeMenu_);
    setMode(mode);
}

FileMenu *TitleBarActions::fileMenu() const
{
    return fileMenu_;
}
ModeMenu *TitleBarActions::modeMenu() const
{
    return modeMenu_;
}

void TitleBarActions::setMode(const QString &mode)
{
    fileMenu_->setMode(mode);
    scrollButton_->setEnabled(mode != "preview");
}

QAction *TitleBarActions::addCommand(const QString &text)
{
    return addAction(text);
}

bool TitleBarActions::isScrollSynchronized() const
{
    return synchronizedAction_ && synchronizedAction_->isChecked();
}
