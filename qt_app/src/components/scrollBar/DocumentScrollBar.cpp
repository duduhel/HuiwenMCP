/* 文档滚动条样式：24px 宽、最小高度 30px、灰色直角滑块与透明轨道。 */
#include "DocumentScrollBar.h"
#include "styles/StyleSheet.h"

DocumentScrollBar::DocumentScrollBar(QWidget *parent) : QScrollBar(Qt::Vertical, parent)
{
    setStyleSheet(loadStyle(QStringLiteral(":/styles/document/DocumentScrollBar.qss")));
}
