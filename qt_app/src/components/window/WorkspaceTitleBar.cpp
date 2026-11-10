#include "WorkspaceTitleBar.h"
#include "cust/framelesswidget.h"
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#ifdef Q_OS_OPENHARMONY
#include <multimodalinput/oh_input_manager.h>
#include <window_manager/oh_display_manager.h>
#endif

WorkspaceTitleBar::WorkspaceTitleBar(QWidget *window)
    : QWidget(window), window_(window), title_(new QLabel(window->windowTitle(), this))
{
    setObjectName(QStringLiteral("workspaceTitleBar"));
    setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    harmonyLogo_ = new QToolButton(this);
    harmonyLogo_->setObjectName(QStringLiteral("harmonyLogo"));
    harmonyLogo_->setIcon(QIcon(QStringLiteral(":/branding/harmonyos_logo.png")));
    harmonyLogo_->setAccessibleName(QStringLiteral("打开关于本机"));
    harmonyLogo_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(harmonyLogo_);
    connect(harmonyLogo_, &QToolButton::clicked, this, &WorkspaceTitleBar::aboutDeviceRequested);
    title_->setMinimumWidth(0);
    title_->setTextFormat(Qt::RichText);
    title_->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(title_, 1);
    auto *windowActions = new FramelessWidget(window_, title_);
    const auto makeButton = [this, layout](const QString &name)
    {
        auto *button = new QToolButton(this);
        button->setAccessibleName(name);
        button->setProperty("titleBarHover", false);
        button->installEventFilter(this);
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
    window_->installEventFilter(this);
    updateMetrics();
}

void WorkspaceTitleBar::updateMetrics()
{
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
    layout()->setContentsMargins(0, inset, vp(16), inset);
    setStyleSheet(
        QStringLiteral(
            "#workspaceTitleBar { background: transparent; }"
            "#workspaceTitleBar QLabel { background: transparent; color: #000000; "
            "padding-left: %1px; font-size: %2px; font-weight: bold; }"
            "#workspaceTitleBar QToolButton { border: none; background: transparent; "
            "padding: 0px %4px 0px %3px; background-origin: content; background-clip: content; }"
            "#workspaceTitleBar QToolButton#harmonyLogo { padding: 0px %5px 0px %6px; }"
            "#workspaceTitleBar QToolButton[titleBarHover=\"true\"] { background-color: #DCE1E8; }"
            "#workspaceTitleBar QToolButton#closeWindow[titleBarHover=\"true\"] "
            "{ background-color: #E86969; }")
            .arg(vp(window_->isMaximized() || window_->isFullScreen() ? 8 : 16))
            .arg(qRound(16 * fontScale))
            .arg(leftPadding)
            .arg(rightPadding)
            .arg(vp(8))
            .arg(vp(16)));
    // 对齐首页：字标左侧 16vp，右侧 8vp，后续标题仍是独立拖动区。
    harmonyLogo_->setFixedSize(vp(132), buttonHeight);
    harmonyLogo_->setIconSize(QSize(vp(108), vp(20)));
    const QSize iconSize(vp(20), vp(20));
    for (auto *button : {maximize_, minimize_, close_})
    {
        button->setFixedSize(buttonWidth, buttonHeight);
        button->setIconSize(iconSize);
    }
    minimize_->setIcon(QIcon(QStringLiteral(":/window-controls/minimize.svg")));
    close_->setIcon(QIcon(QStringLiteral(":/window-controls/close.svg")));
    updateWindowState();
}

void WorkspaceTitleBar::refreshHover()
{
    bool valid = isVisible() && window_->isActiveWindow();
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
        button->setProperty("titleBarHover", inside);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
        changed = true;
    }
    if (changed)
        window_->update(QRect(mapTo(window_, QPoint()), size()));
    // 仅悬停在三键期间检查；离开、失焦或隐藏后停止。
    if (hovered)
    {
        if (!hoverTimer_.isActive())
            hoverTimer_.start();
    }
    else
        hoverTimer_.stop();
}

void WorkspaceTitleBar::updateWindowState()
{
    const bool restore = window_->isMaximized() || window_->isFullScreen();
    // 鸿蒙字标独立点击；标题文字继续承载拖动与双击。
    harmonyLogo_->setVisible(restore);
    const QString branding =
        restore ? QStringLiteral("×&nbsp; ")
                : QStringLiteral("<img src=\":/branding/app_icon.png\" width=\"%1\" height=\"%1\" "
                                 "style=\"vertical-align: middle;\"> &nbsp;")
                      .arg(qRound(20 * scale_));
    title_->setText(branding + window_->windowTitle().toHtmlEscaped());
    title_->setAccessibleName(window_->windowTitle());
    const QString text = restore ? QStringLiteral("还原") : QStringLiteral("最大化");
    maximize_->setIcon(QIcon(restore ? QStringLiteral(":/window-controls/recover.svg")
                                     : QStringLiteral(":/window-controls/maximize.svg")));
    maximize_->setAccessibleName(text);
}

bool WorkspaceTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window_)
    {
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
