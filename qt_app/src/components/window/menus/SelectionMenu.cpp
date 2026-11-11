/* 查看模式、业务模式和滚动设置复用同一绘制，统一圆点大小、颜色和右侧间距。 */
#include "SelectionMenu.h"
#include <QAction>
#include <QPainter>

SelectionMenu::SelectionMenu(QWidget *parent) : QMenu(parent)
{
    setToolTipsVisible(true);
    setStyleSheet(QStringLiteral("QMenu { background: #F7F8FA; border: none; padding: 8px; "
                                 "color: #34445C; font-size: 28px; font-weight: normal; }"
                                 "QMenu::item { min-height: 40px; padding: 16px 64px 16px 20px; }"
                                 "QMenu::item:selected { background: #E5E8ED; }"
                                 "QMenu::item:disabled { color: #A0A8B4; }"
                                 "QMenu::indicator { width: 0px; height: 0px; image: none; }"
                                 "QMenu::right-arrow { width: 0px; height: 0px; image: none; }"));
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
