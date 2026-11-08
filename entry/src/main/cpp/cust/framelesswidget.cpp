#include "framelesswidget.h"


#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>
#include <QWindow>
#include <QWidget>

FramelessWidget::FramelessWidget(QWidget *window, QWidget *surface)
    : QObject(surface), widget(window), dragSurface(surface)
{
    // 标题区与按钮是兄弟控件。仅接收标题区鼠标事件，避免还原窗口后误拦按钮点击。
    dragSurface->installEventFilter(this);
    widget->installEventFilter(this);

    manualMoveTimer.setInterval(16);
    connect(&manualMoveTimer, &QTimer::timeout, this, [this]() {
        if (!movePending || !(QApplication::mouseButtons() & Qt::LeftButton)) {
            stopManualMove();
            return;
        }

        const QPoint cursorPos = QCursor::pos();
        if (!manualMoveStarted
            && (cursorPos - pressGlobalPos).manhattanLength() < QApplication::startDragDistance()) {
            return;
        }
        manualMoveStarted = true;
        widget->move(pressWindowPos + cursorPos - pressGlobalPos);
    });
}

void FramelessWidget::setSystemWindowActionsEnabled(bool enabled)
{
    systemWindowActionsEnabled = enabled;
    stopManualMove();
}

void FramelessWidget::stopManualMove()
{
    manualMoveTimer.stop();
    manualMoveStarted = false;
    movePending = false;
}

bool FramelessWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::WindowStateChange || event->type() == QEvent::UngrabMouse) {
        stopManualMove();
    }
    if (watched != dragSurface || !widget->isVisible()) {
        return QObject::eventFilter(watched, event);
    }

    const bool isMouseEvent = event->type() == QEvent::MouseButtonDblClick
        || event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease
        || event->type() == QEvent::MouseMove;
    if (!isMouseEvent) {
        return QObject::eventFilter(watched, event);
    }

    const auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (mouseEvent->button() == Qt::LeftButton) {
            return handleTitleDoubleClick();
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        if (mouseEvent->button() == Qt::LeftButton) {
            return handleTitleMousePress(mouseEvent->globalPos());
        }
    }

    if (event->type() == QEvent::MouseMove && movePending) {
        return handleTitleMouseMove(mouseEvent);
    }

    if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
        const bool wasPending = movePending;
        stopManualMove();
        if (wasPending) {
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}
bool FramelessWidget::handleTitleDoubleClick()
{
    stopManualMove();
    if (!systemWindowActionsEnabled) {
        // 登录页是固定尺寸窗口，不触发系统最大化。
        return true;
    }
    const bool restore = widget->isMaximized() || widget->isFullScreen();
    // 鸿蒙无边框窗口适配：Qt showMaximized/showNormal 由 OHOS QPA 分别映射到
    // Window.maximize() 与 Window.recover()，保留系统最大化/还原动画和窗口记忆。
    if (restore) {
        widget->showNormal();
    } else {
        widget->showMaximized();
    }
    return true;
}

bool FramelessWidget::handleTitleMousePress(const QPoint &globalPos)
{
    // 先等待超过 Qt 标准拖动距离，避免第一次点击的系统移动与双击最大化竞争。
    pressGlobalPos = globalPos;
    pressWindowPos = widget->pos();
    movePending = true;
    if (!systemWindowActionsEnabled) {
        // 登录页不调用 startSystemMove，避免贴边触发系统全屏/分屏。
        // 定时读取全局鼠标坐标，鼠标快速移出窗口后仍可继续拖动。
        manualMoveStarted = false;
        manualMoveTimer.start();
    }
    return true;
}

bool FramelessWidget::handleTitleMouseMove(const QMouseEvent *mouseEvent)
{
    if (!(mouseEvent->buttons() & Qt::LeftButton)) {
        stopManualMove();
        return true;
    }
    if (!systemWindowActionsEnabled || manualMoveTimer.isActive()) {
        return true;
    }
    if ((mouseEvent->globalPos() - pressGlobalPos).manhattanLength()
        < QApplication::startDragDistance()) {
        return true;
    }

    movePending = false;
    // 鸿蒙核心 API：QWindow::startSystemMove() 由 OHOS QPA 调用 Window.startMoving()；
    // 超过拖动阈值后系统接管，即使鼠标快速移出窗口也能继续移动和触发系统贴边布局。
    QWindow *window = widget->windowHandle();
    const bool started = window && window->startSystemMove();
    if (!started && !widget->isMaximized() && !widget->isFullScreen()) {
        movePending = true;
        manualMoveTimer.start();
    }
    return true;
}
