/* 查看模式、业务模式和滚动设置复用同一绘制，统一圆点大小、颜色和右侧间距。 */
#include "SelectionMenu.h"
#include "styles/StyleSheet.h"
#include <QAction>
#include <QPainter>

SelectionMenu::SelectionMenu(QWidget *parent) : QMenu(parent)
{
    setToolTipsVisible(true);
    setStyleSheet(loadStyle(QStringLiteral(":/styles/window/SelectionMenu.qss")));
}

void SelectionMenu::paintEvent(QPaintEvent *event)
{
    QMenu::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    for (auto *action : actions())
    {
        if (!action->isVisible() || action->isSeparator())
            continue;
        const QRect row = actionGeometry(action);
        if (action->isChecked())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#3983F7")));
            painter.drawEllipse(QPointF(row.right() - 24, row.center().y()), 7, 7);
        }
        if (action->menu())
        {
            painter.setPen(QPen(QColor(action->isEnabled() ? "#65758C" : "#A0A8B4"), 2));
            const QPointF center(row.right() - 7, row.center().y());
            painter.drawLine(center + QPointF(-3, -5), center + QPointF(2, 0));
            painter.drawLine(center + QPointF(2, 0), center + QPointF(-3, 5));
        }
    }
}
