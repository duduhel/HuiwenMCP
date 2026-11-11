/* 中部分割线上的悬浮转化按钮；宽度随状态文字调整，不占用整行。 */
#include "ConvertButton.h"
#include "styles/StyleSheet.h"
#include <QFontMetrics>
#include <QPainterPath>
#include <QRegion>
#include <QWindow>
#include <algorithm>

ConvertButton::ConvertButton(QWidget *parent) : QPushButton(QStringLiteral("转化"), parent)
{
#ifdef Q_OS_OPENHARMONY
    // CEF 使用原生子节点，按钮也创建同级原生节点，覆盖范围仅限按钮实体。
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
#endif
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(loadStyle(QStringLiteral(":/styles/workspace/ConvertButton.qss")));
    setState(false, false);
}

void ConvertButton::setState(bool hasDocument, bool busy, bool retry)
{
    setEnabled(hasDocument && !busy);
    setText(busy ? QStringLiteral("转化中…") : retry ? QStringLiteral("重新转化") : QStringLiteral("转化"));
    const QFontMetrics metrics = fontMetrics();
    setFixedSize(std::max(busy ? 128 : 96, metrics.horizontalAdvance(text()) + 24),
                 std::max(40, metrics.height() + 4));
    QPainterPath outline;
    outline.addRoundedRect(QRectF(rect()), 8, 8);
    setMask(QRegion(outline.toFillPolygon().toPolygon()));
}

void ConvertButton::raiseAbovePreview()
{
    raise();
#ifdef Q_OS_OPENHARMONY
    // 显式提升原生层级；即使 Qt 兄弟控件顺序未变，也要位于新建的 CEF 节点之上。
    if (isVisible() && windowHandle())
        windowHandle()->raise();
#endif
}
