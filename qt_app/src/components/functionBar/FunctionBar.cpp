/* 顶部文件与滚动入口；菜单选中项是同步状态的唯一来源，工作区接收变化后控制联动。 */
#include "FunctionBar.h"
#include "FileMenu.h"
#include "ModeMenu.h"
#include "SelectionMenu.h"
#include "sidebars/common/SidebarIcons.h"
#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QLayout>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <algorithm>
#ifdef Q_OS_OPENHARMONY
#include <window_manager/oh_display_manager.h>
#endif

FunctionBar::FunctionBar(const QString &mode, QWidget *parent)
    : QToolBar(parent), fileMenu_(new FileMenu(mode, this)), modeMenu_(new ModeMenu(this))
{
    setObjectName(QStringLiteral("workspaceFunctionBar"));
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setFixedHeight(72);
    setContentsMargins(0, 0, 0, 0);
    setAutoFillBackground(false);
    updateAppearance();

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
        // 先应用继承样式再测量 28px 字体，避免按默认小字号分配宽度而裁切文字。
        button->ensurePolished();
        const int textWidth = std::max(button->fontMetrics().horizontalAdvance(text),
                                       button->fontMetrics().boundingRect(text).width());
        const QMargins margins = layout()->contentsMargins();
        button->setFixedSize(std::max(button->sizeHint().width(), textWidth + 40),
                             height() - margins.top() - margins.bottom());
        addWidget(button);
        return button;
    };
    navigationToggle_ = new QToolButton(this);
    navigationToggle_->setObjectName("navigationSidebarToggle");
    navigationToggle_->setIcon(sidebarIcon());
    navigationToggle_->setIconSize(QSize(28, 28));
    navigationToggle_->setFixedSize(48, 48);
    navigationToggle_->setCheckable(true);
    navigationToggle_->setCursor(Qt::PointingHandCursor);
    navigationToggle_->setAccessibleName(QStringLiteral("开关工作区导航侧栏"));
    setNavigationSidebarVisible(false);
    connect(navigationToggle_, &QToolButton::toggled, this, [this](bool visible) {
        setNavigationSidebarVisible(visible);
        emit navigationSidebarToggled(visible);
    });
    addWidget(navigationToggle_);
    addMenuButton(QStringLiteral("文件"), fileMenu_);
    // 保持入口位置稳定；单栏预览下禁用滚动设置，不移走后方的模式菜单。
    {
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
        connect(synchronizedAction_, &QAction::toggled, this, &FunctionBar::scrollSyncChanged);
        scrollButton_ = addMenuButton(QStringLiteral("滚动设置"), menu);
    }
    addMenuButton(QStringLiteral("模式"), modeMenu_);
    setMode(mode);
    auto *space = new QWidget(this);
    space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    trailingSpacer_ = addWidget(space);
}

void FunctionBar::updateAppearance()
{
    qreal scale = 1;
#ifdef Q_OS_OPENHARMONY
    float density = 1;
    OH_NativeDisplayManager_GetDefaultDisplayDensityPixels(&density);
    NativeDisplayManager_DisplaysInfo *displays = nullptr;
    if (OH_NativeDisplayManager_CreateAllDisplays(&displays) == DISPLAY_MANAGER_OK && displays)
    {
        for (uint32_t i = 0; i < displays->displaysLength; ++i)
        {
            const auto &display = displays->displaysInfo[i];
            if (QString::fromLocal8Bit(display.name) == window()->screen()->name())
            {
                density = display.densityPixels;
                break;
            }
        }
        OH_NativeDisplayManager_DestroyAllDisplays(displays);
    }
    // 与标题栏一致：将首页的 vp 换算到 Qt 坐标，避免与 Qt 像素比重复缩放。
    scale = (density > 0 ? density : 1) / window()->devicePixelRatioF();
#endif
    const int radius = qRound(24 * scale);
    setStyleSheet(
        QStringLiteral(
            "QToolBar#workspaceFunctionBar { background: #F2F3F5; border: none; "
            "border-top-left-radius: %1px; border-top-right-radius: %1px; "
            "border-bottom-left-radius: 0px; border-bottom-right-radius: 0px; "
            "margin: 0; padding: 0; spacing: 0; }"
            "QToolBar#workspaceFunctionBar QToolButton { background: transparent; border: none; "
            "border-radius: 10px; margin: 0; padding: 0 20px; "
            "color: #34445C; font-size: 28px; font-weight: normal; }"
            "QToolBar#workspaceFunctionBar QToolButton[selected=\"true\"] "
            "{ background: #DCE1E8; }"
            "QToolBar#workspaceFunctionBar QToolButton#navigationSidebarToggle "
            "{ padding: 0; border-radius: 8px; }"
            "QToolBar#workspaceFunctionBar QToolButton#navigationSidebarToggle:checked "
            "{ background: #E5E5E5; }"
            "QToolBar#workspaceFunctionBar QToolButton::menu-indicator "
            "{ image: none; width: 0; height: 0; }")
            .arg(radius));
    // 工具栏保持原来的实色，仅顶部圆角缺口透出背景。
    layout()->setContentsMargins(12, 8, 12, 8);
    layout()->setSpacing(0);
}

bool FunctionBar::event(QEvent *event)
{
    if (event->type() == QEvent::Show || event->type() == QEvent::ScreenChangeInternal ||
        event->type() == QEvent::ApplicationFontChange)
        updateAppearance();
    return QToolBar::event(event);
}

FileMenu *FunctionBar::fileMenu() const { return fileMenu_; }
ModeMenu *FunctionBar::modeMenu() const
{
    return modeMenu_;
}

void FunctionBar::setMode(const QString &mode)
{
    fileMenu_->setMode(mode);
    scrollButton_->setEnabled(mode != "preview");
}

QAction *FunctionBar::addCommand(const QString &text)
{
    auto *action = new QAction(text, this);
    insertAction(trailingSpacer_, action); // 保存等命令放在弹性留白之前，保持靠左。
    return action;
}

bool FunctionBar::isScrollSynchronized() const
{
    return synchronizedAction_ && synchronizedAction_->isChecked();
}

void FunctionBar::setNavigationSidebarVisible(bool visible)
{
    navigationToggle_->setChecked(visible);
    navigationToggle_->setToolTip(visible ? QStringLiteral("收起工作区导航")
                                          : QStringLiteral("展开工作区导航"));
}
