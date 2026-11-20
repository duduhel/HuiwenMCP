/* 工作区自绘标题栏：标识与窗口三键之间承载菜单，留白继续作为窗口拖动区。 */
#include "WorkspaceTitleBar.h"
#include "TitleBarActions.h"
#include "cust/framelesswidget.h"
#include "styles/StyleSheet.h"
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidgetAction>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#include <multimodalinput/oh_input_manager.h>
#include <window_manager/oh_display_manager.h>
#endif

WorkspaceTitleBar::WorkspaceTitleBar(const QString &mode, QWidget *window)
    : QWidget(window), window_(window), title_(new QLabel(window->windowTitle(), this)),
      titleActions_(new TitleBarActions(mode, this))
{
    setObjectName(QStringLiteral("workspaceTitleBar"));
    setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    appIcon_ = new QToolButton(this);
    appIcon_->setObjectName(QStringLiteral("appIcon"));
    appIcon_->setIcon(QIcon(QStringLiteral(":/branding/app_icon.png")));
    appIcon_->setAccessibleName(QStringLiteral("打开关于本机"));
    appIcon_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(appIcon_);
    connect(appIcon_, &QToolButton::clicked, this, &WorkspaceTitleBar::aboutDeviceRequested);
    title_->setMinimumWidth(0);
    title_->setTextFormat(Qt::PlainText);
    title_->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(title_);
    layout->addWidget(titleActions_);
    auto *dragSpace = new QWidget(this);
    layout->addWidget(dragSpace, 1);
    new FramelessWidget(window_, dragSpace);
    auto *windowActions = new FramelessWidget(window_, title_);
    const auto makeButton = [this, layout](const QString &name)
    {
        auto *button = new QToolButton(this);
        button->setAccessibleName(name);
        button->setProperty("titleBarHover", false);
        button->installEventFilter(this);
        connect(button, &QToolButton::pressed, this, &WorkspaceTitleBar::hideMaximizeMenu);
        layout->addWidget(button);
        return button;
    };
    maximize_ = makeButton(QStringLiteral("最大化"));
    minimize_ = makeButton(QStringLiteral("最小化"));
    close_ = makeButton(QStringLiteral("关闭工作区"));
    close_->setObjectName(QStringLiteral("closeWindow"));
    connect(minimize_, &QToolButton::clicked, window_, &QWidget::showMinimized);
    connect(maximize_, &QToolButton::clicked, windowActions, &FramelessWidget::toggleMaximized);
    connect(close_, &QToolButton::clicked, this, &WorkspaceTitleBar::closeRequested);
    hoverTimer_.setInterval(50);
    connect(&hoverTimer_, &QTimer::timeout, this, &WorkspaceTitleBar::refreshHover);
    maximizeMenuDelay_.setSingleShot(true);
    maximizeMenuDelay_.setInterval(400);
    connect(&maximizeMenuDelay_, &QTimer::timeout, this, &WorkspaceTitleBar::showMaximizeMenu);
    window_->installEventFilter(this);
#ifdef Q_OS_OPENHARMONY
    connect(&QtBridge::instance(), &QtBridge::titleBarContrastChanged, this,
            &WorkspaceTitleBar::setLightForeground);
#endif
    updateMetrics();
}

void WorkspaceTitleBar::setLightForeground(bool light)
{
    if (lightForeground_ == light)
        return;
    lightForeground_ = light;
    setProperty("lightForeground", light);
    titleActions_->setProperty("lightForeground", light);
    // 只重设前景样式，不重算窗口布局或关闭正在使用的菜单。
    for (QWidget *widget : {static_cast<QWidget *>(title_), static_cast<QWidget *>(titleActions_)})
    {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
    for (auto *button : titleActions_->findChildren<QToolButton *>())
    {
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
    updateWindowState();
}

TitleBarActions *WorkspaceTitleBar::titleActions() const
{
    return titleActions_;
}

void WorkspaceTitleBar::updateMetrics()
{
    hideMaximizeMenu();
    if (maximizeMenu_)
    {
        maximizeMenu_->deleteLater();
        maximizeMenu_ = nullptr;
    }
    qreal fontScale = 1;
#ifdef Q_OS_OPENHARMONY
    float density = 1, scaledDensity = 1;
    OH_NativeDisplayManager_GetDefaultDisplayDensityPixels(&density);
    OH_NativeDisplayManager_GetDefaultDisplayScaledDensity(&scaledDensity);
    uint64_t defaultId = 0;
    OH_NativeDisplayManager_GetDefaultDisplayId(&defaultId);
    displayId_ = static_cast<qint32>(defaultId);
    NativeDisplayManager_DisplaysInfo *displays = nullptr;
    if (OH_NativeDisplayManager_CreateAllDisplays(&displays) == DISPLAY_MANAGER_OK && displays)
    {
        for (uint32_t i = 0; i < displays->displaysLength; ++i)
        {
            const auto &display = displays->displaysInfo[i];
            if (QString::fromLocal8Bit(display.name) == window_->screen()->name())
            {
                density = display.densityPixels;
                scaledDensity = display.scaledDensity;
                displayId_ = static_cast<qint32>(display.id);
                break;
            }
        }
        OH_NativeDisplayManager_DestroyAllDisplays(displays);
    }
    // ArkUI 使用 vp/fp；除去 Qt 已应用的像素比，避免再次缩放。
    scale_ = (density > 0 ? density : 1) / window_->devicePixelRatioF();
    fontScale = (scaledDensity > 0 ? scaledDensity : 1) / window_->devicePixelRatioF();
#endif
    const auto vp = [this](qreal value) { return qRound(value * scale_); };
    const int barHeight = vp(37), inset = vp(6);
    const int buttonWidth = vp(48), buttonHeight = barHeight - 2 * inset;
    const int leftPadding = (buttonWidth - buttonHeight) / 2;
    const int rightPadding = buttonWidth - buttonHeight - leftPadding;
    setFixedHeight(barHeight);
    // 保留系统缩放热区的避让，同时让按钮上下边距相等。
    // 三键仍按原尺寸垂直居中；菜单使用标题栏全高，避免 26px 文字被内边距裁切。
    layout()->setContentsMargins(vp(8), 0, vp(16), 0);
    setStyleSheet(loadStyle(QStringLiteral(":/styles/window/WorkspaceTitleBar.qss"))
                      .arg(0)
                      .arg(qRound(16 * fontScale))
                      .arg(leftPadding)
                      .arg(rightPadding)
                      .arg(vp(8))
                      .arg(vp(8)));
    // 对齐首页：应用图标 20vp，两侧各留 8vp，标题文字独立承载拖动。
    appIcon_->setFixedSize(vp(36), buttonHeight);
    appIcon_->setIconSize(QSize(vp(20), vp(20)));
    const QSize iconSize(vp(20), vp(20));
    for (auto *button : {maximize_, minimize_, close_})
    {
        button->setFixedSize(buttonWidth, buttonHeight);
        button->setIconSize(iconSize);
    }
    updateWindowState();
}

void WorkspaceTitleBar::refreshHover()
{
    bool valid = isVisible() &&
                 (window_->isActiveWindow() || (maximizeMenu_ && maximizeMenu_->isActiveWindow()));
    QPoint position;
#ifdef Q_OS_OPENHARMONY
    // QCursor 在 CEF 原生区域内可能不再更新，直接查询系统鼠标位置。
    int32_t displayId = -1;
    double x = 0, y = 0;
    valid = valid && OH_Input_GetPointerLocation(&displayId, &x, &y) == INPUT_SUCCESS &&
            displayId == displayId_;
    if (valid)
        position = window_->screen()->geometry().topLeft() +
                   QPoint(qRound(x / window_->devicePixelRatioF()),
                          qRound(y / window_->devicePixelRatioF()));
#else
    position = QCursor::pos();
#endif
    bool hovered = false, changed = false;
    for (auto *button : {maximize_, minimize_, close_})
    {
        const bool inside = valid && button->rect().contains(button->mapFromGlobal(position));
        hovered = hovered || inside;
        if (button->property("titleBarHover").toBool() == inside)
            continue;
        if (button == maximize_)
        {
            if (inside && !window_->isMaximized() && !window_->isFullScreen())
                maximizeMenuDelay_.start();
            else
                maximizeMenuDelay_.stop();
        }
        button->setProperty("titleBarHover", inside);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
        changed = true;
    }
    if (changed)
    {
        updateWindowState();
        window_->update(QRect(mapTo(window_, QPoint()), size()));
    }
    if (maximizeMenu_ && maximizeMenu_->isVisible())
    {
        // 允许经过按钮和菜单之间的 6vp 间隙，移入菜单后继续检查原生鼠标位置。
        const int gap = qRound(6 * scale_);
        const QRect anchor(maximize_->mapToGlobal(QPoint()), maximize_->size());
        if (!valid || window_->isMaximized() || window_->isFullScreen() ||
            (!anchor.adjusted(-gap, -gap, gap, gap).contains(position) &&
             !maximizeMenu_->frameGeometry().contains(position)))
            hideMaximizeMenu();
        hovered = hovered || maximizeMenu_->isVisible();
    }
    // 仅三键或放大菜单交互期间检查；离开、失焦或隐藏后停止。
    if (hovered)
    {
        if (!hoverTimer_.isActive())
            hoverTimer_.start();
    }
    else
        hoverTimer_.stop();
}

void WorkspaceTitleBar::showMaximizeMenu()
{
    // 与按钮图标保持一致：只有普通窗口、显示“放大”图标时才提供模式选择。
    if (!isVisible() || !window_->isActiveWindow() || window_->isMaximized() ||
        window_->isFullScreen() || !maximize_->property("titleBarHover").toBool())
        return;
    const auto vp = [this](qreal value) { return qRound(value * scale_); };
    if (!maximizeMenu_)
    {
        maximizeMenu_ = new QMenu(this);
        maximizeMenu_->setObjectName(QStringLiteral("windowModeMenu"));
        maximizeMenu_->setWindowFlag(Qt::FramelessWindowHint);
        maximizeMenu_->setAttribute(Qt::WA_TranslucentBackground);
        maximizeMenu_->setAttribute(Qt::WA_ShowWithoutActivating);
        maximizeMenu_->setStyleSheet(
            loadStyle(QStringLiteral(":/styles/window/WindowModeMenu.qss")).arg(vp(14)).arg(vp(8)));
        auto *content = new QWidget(maximizeMenu_);
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(vp(16), vp(12), vp(16), vp(12));
        layout->setSpacing(vp(12));
        layout->addWidget(new QLabel(QStringLiteral("放大模式"), content));
        auto *choices = new QHBoxLayout;
        choices->setSpacing(vp(12));
        for (const bool immersive : {false, true})
        {
            auto *choice = new QToolButton(content);
            choice->setText(immersive ? QStringLiteral("沉浸式全屏") : QStringLiteral("全屏"));
            choice->setAccessibleName(choice->text());
            choice->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            choice->setFocusPolicy(Qt::StrongFocus);
            choice->setFixedSize(vp(112), vp(108));
            choice->setIconSize(QSize(vp(96), vp(64)));
            choice->setIcon(QIcon(immersive
                                      ? QStringLiteral(":/window-modes/immersive-fullscreen.svg")
                                      : QStringLiteral(":/window-modes/fullscreen.svg")));
            connect(choice, &QToolButton::clicked, this,
                    [this, immersive]
                    {
                        hideMaximizeMenu();
                        if (immersive)
                            window_->showFullScreen();
                        else
                            window_->showMaximized();
                    });
            choices->addWidget(choice);
        }
        layout->addLayout(choices);
        auto *action = new QWidgetAction(maximizeMenu_);
        action->setDefaultWidget(content);
        maximizeMenu_->addAction(action);
    }
    maximizeMenu_->popup(
        maximize_->mapToGlobal(QPoint((maximize_->width() - maximizeMenu_->sizeHint().width()) / 2,
                                      maximize_->height() + vp(6))));
}

void WorkspaceTitleBar::hideMaximizeMenu()
{
    maximizeMenuDelay_.stop();
    if (maximizeMenu_)
        maximizeMenu_->hide();
}

void WorkspaceTitleBar::updateWindowState()
{
    const bool restore = window_->isMaximized() || window_->isFullScreen();
    // 应用图标始终独立显示并支持点击，窗口状态只影响三键图标。
    title_->setText(window_->windowTitle());
    title_->setAccessibleName(window_->windowTitle());
    const QString text = restore ? QStringLiteral("还原") : QStringLiteral("最大化");
    const auto setIcon = [this](QToolButton *button, const QString &path)
    {
        QPixmap pixels = QIcon(path).pixmap(button->iconSize());
        QPainter painter(&pixels);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        const bool light = lightForeground_ && !button->property("titleBarHover").toBool();
        painter.fillRect(pixels.rect(), light ? Qt::white : Qt::black);
        painter.end();
        button->setIcon(QIcon(pixels));
    };
    setIcon(maximize_, restore ? QStringLiteral(":/window-controls/recover.svg")
                               : QStringLiteral(":/window-controls/maximize.svg"));
    setIcon(minimize_, QStringLiteral(":/window-controls/minimize.svg"));
    setIcon(close_, QStringLiteral(":/window-controls/close.svg"));
    maximize_->setAccessibleName(text);
}

bool WorkspaceTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window_)
    {
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize)
            hideMaximizeMenu();
        if (event->type() == QEvent::WindowTitleChange)
            updateWindowState();
        if (event->type() == QEvent::Show || event->type() == QEvent::ScreenChangeInternal ||
            event->type() == QEvent::ApplicationFontChange ||
            event->type() == QEvent::WindowStateChange)
            updateMetrics();
        if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate ||
            event->type() == QEvent::Hide || event->type() == QEvent::WindowStateChange)
            refreshHover();
    }
    else if (watched == maximize_ || watched == minimize_ || watched == close_)
    {
        if (event->type() == QEvent::ToolTip)
            return true;
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave)
            refreshHover();
    }
    return QWidget::eventFilter(watched, event);
}
