/* 从编入程序的 Qt 资源加载 QSS；控件继续在本地替换缩放、颜色等动态参数。 */
#pragma once
#include <QFile>
#include <QString>
#include <QtGlobal>

inline QString loadStyle(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot load Qt stylesheet resource: %s", qPrintable(resourcePath));
    return QString::fromUtf8(file.readAll());
}
