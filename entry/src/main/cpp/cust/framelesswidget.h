#ifndef FRAMELESSWIDGET_H
#define FRAMELESSWIDGET_H

#include <QObject>
#include <QPoint>
#include <QTimer>

class QWidget;
class QMouseEvent;
class FramelessWidget : public QObject
{
    Q_OBJECT
public:
    FramelessWidget(QWidget *window, QWidget *dragSurface);
    void setSystemWindowActionsEnabled(bool enabled);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void stopManualMove();
    // eventFilter 拆出的标题栏事件处理器，均返回 true 表示事件已消费。
    bool handleTitleDoubleClick();
    bool handleTitleMousePress(const QPoint &globalPos);
    bool handleTitleMouseMove(const QMouseEvent *mouseEvent);

    QWidget *widget;
    QWidget *dragSurface;
    QPoint pressGlobalPos;
    QPoint pressWindowPos;
    QTimer manualMoveTimer;
    bool systemWindowActionsEnabled = true;
    bool manualMoveStarted = false;
    bool movePending = false;
};

#endif // FRAMELESSWIDGET_H
