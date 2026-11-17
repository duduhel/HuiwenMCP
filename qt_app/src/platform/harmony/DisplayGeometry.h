/* 鸿蒙多屏坐标查询：复用通达的屏幕匹配和原点对齐规则，供截图起窗使用。 */
#pragma once
#include <QPointer>
#include <QRect>
#include <QScreen>

namespace huiwen::window
{
struct DisplayGeometry
{
    QPointer<QScreen> screen;
    QRect nativeRect;
    QRect availableRect;
    QPoint toGlobal(const QPointF &localPoint) const;
    QRect toGlobal(const QRect &localRect) const;
    QRect geometry() const;
};

QScreen *screenForDisplay(qint64 displayId, QString &error);
DisplayGeometry displayGeometry(qint64 displayId, QString &error);
} // namespace huiwen::window
