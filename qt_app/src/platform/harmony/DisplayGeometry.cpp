/* 截图多屏适配：系统偏移先对齐 Qt 主屏原点，屏内坐标再按目标屏 DPI 换算。 */
#include "DisplayGeometry.h"
#include <QDebug>
#include <QGuiApplication>
#include <private/qhighdpiscaling_p.h>
#include <qpa/qplatformscreen.h>
#include <window_manager/oh_display_manager.h>

namespace huiwen::window
{
QScreen *screenForDisplay(qint64 displayId, QString &error)
{
    error.clear();
    NativeDisplayManager_DisplayInfo *display = nullptr;
    const auto code = OH_NativeDisplayManager_CreateDisplayById(displayId, &display);
    if (code != DISPLAY_MANAGER_OK || !display || !display->isAlive ||
        display->state == DISPLAY_MANAGER_DISPLAY_STATE_OFF)
    {
        if (display)
            OH_NativeDisplayManager_DestroyDisplay(display);
        error = QStringLiteral("display-unavailable id=%1 code=%2").arg(displayId).arg(int(code));
        return nullptr;
    }
    const QString name = QString::fromUtf8(display->name);
    const QSize pixels(display->width, display->height);
    OH_NativeDisplayManager_DestroyDisplay(display);
    QScreen *matched = nullptr;
    int count = 0;
    for (QScreen *screen : QGuiApplication::screens())
    {
        // 平台尺寸已是物理像素，不再乘 DPR；同名同尺寸存在歧义时不猜屏。
        if (screen->handle() && screen->name() == name &&
            screen->handle()->geometry().size() == pixels)
        {
            matched = screen;
            ++count;
        }
    }
    if (count != 1)
    {
        error = QStringLiteral("qt-screen-match id=%1 name=%2 matches=%3")
                    .arg(displayId)
                    .arg(name)
                    .arg(count);
        return nullptr;
    }
    return matched;
}

QPoint DisplayGeometry::toGlobal(const QPointF &localPoint) const
{
    return screen && nativeRect.isValid()
               ? nativeRect.topLeft() +
                     QHighDpi::fromNativeLocalPosition(localPoint, screen.data()).toPoint()
               : QPoint();
}

QRect DisplayGeometry::toGlobal(const QRect &localRect) const
{
    return screen && nativeRect.isValid()
               ? QHighDpi::fromNativeLocalPosition(localRect, screen.data())
                     .translated(nativeRect.topLeft())
               : QRect();
}

QRect DisplayGeometry::geometry() const
{
    return toGlobal(QRect(QPoint(), nativeRect.size()));
}

DisplayGeometry displayGeometry(qint64 displayId, QString &error)
{
    QScreen *screen = screenForDisplay(displayId, error);
    if (!screen)
        return {};
    if (__builtin_available(ohos 20, *))
    {
        int32_t x = 0, y = 0;
        const auto positionCode = OH_NativeDisplayManager_GetDisplayPosition(displayId, &x, &y);
        if (positionCode != DISPLAY_MANAGER_OK)
        {
            error = QStringLiteral("display-position-failed code=%1").arg(int(positionCode));
            return {};
        }
        NativeDisplayManager_DisplayInfo *primary = nullptr;
        const auto primaryCode = OH_NativeDisplayManager_CreatePrimaryDisplay(&primary);
        const qint64 primaryId =
            primaryCode == DISPLAY_MANAGER_OK && primary ? qint64(primary->id) : -1;
        if (primary)
            OH_NativeDisplayManager_DestroyDisplay(primary);
        if (primaryId < 0)
        {
            error = QStringLiteral("primary-display-query-failed code=%1").arg(int(primaryCode));
            return {};
        }
        QScreen *primaryScreen =
            primaryId == displayId ? screen : screenForDisplay(primaryId, error);
        if (!primaryScreen)
            return {};
        const QPoint primaryOrigin = primaryScreen->handle()->geometry().topLeft();
        DisplayGeometry result{
            screen, QRect(primaryOrigin + QPoint(x, y), screen->handle()->geometry().size()), {}};
        result.availableRect = result.geometry();
        // 当前 SDK 已提供同一可用区域的 NDK 查询，无需引入 JS Promise 桥。
        NativeDisplayManager_Rect *area = nullptr;
        const auto areaCode = OH_NativeDisplayManager_CreateAvailableArea(displayId, &area);
        const QRect localArea =
            area ? QRect(area->left, area->top, area->width, area->height) : QRect();
        if (area)
            OH_NativeDisplayManager_DestroyAvailableArea(area);
        if (areaCode == DISPLAY_MANAGER_OK && localArea.isValid() &&
            QRect(QPoint(), result.nativeRect.size()).contains(localArea))
            result.availableRect = result.toGlobal(localArea);
        else
            qWarning() << "HUIWEN_DISPLAY: available-area-fallback display=" << displayId
                       << "code=" << int(areaCode) << "area=" << localArea;
        qInfo() << "HUIWEN_DISPLAY: display=" << displayId << "primary=" << primaryId
                << "primaryOrigin=" << primaryOrigin << "relativeOrigin=" << QPoint(x, y)
                << "nativeRect=" << result.nativeRect << "qtRect=" << result.geometry();
        return result;
    }
    error = QStringLiteral("display-position-requires-api-20");
    return {};
}
} // namespace huiwen::window
