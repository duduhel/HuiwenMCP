/* 导航侧栏开关矢量图标；沿用 24px 设计坐标，不增加 SVG 运行时依赖。 */
#pragma once
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>

inline QIcon sidebarIcon()
{
    QPixmap image(96, 96);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    const QColor color("#666666");
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(QRectF(3, 3, 18, 18), 2, 2);
    painter.drawLine(QPointF(9, 4), QPointF(9, 20));
    painter.end();
    return QIcon(image);
}
