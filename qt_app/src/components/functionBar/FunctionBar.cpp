/* 工作区功能条只管理侧栏及后续业务入口，不持有标题栏菜单或菜单状态。 */
#include "FunctionBar.h"
#include "sidebars/common/SidebarIcons.h"
#include "styles/StyleSheet.h"
#include <QEvent>
#include <QLayout>
#include <QScreen>
#include <QToolButton>
#ifdef Q_OS_OPENHARMONY
#include <window_manager/oh_display_manager.h>
#endif

FunctionBar::FunctionBar(QWidget *parent) : QToolBar(parent)
{
    setObjectName(QStringLiteral("workspaceFunctionBar"));
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setFixedHeight(72);
    setContentsMargins(0, 0, 0, 0);
    setAutoFillBackground(false);
    updateAppearance();

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
    auto *space = new QWidget(this);
    space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(space);
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
    setStyleSheet(loadStyle(QStringLiteral(":/styles/functionBar/FunctionBar.qss")).arg(radius));
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

void FunctionBar::setNavigationSidebarVisible(bool visible)
{
    navigationToggle_->setChecked(visible);
    navigationToggle_->setToolTip(visible ? QStringLiteral("收起工作区导航")
                                          : QStringLiteral("展开工作区导航"));
}
