/* 已实现的工作区能力；菜单、文件选择和实际切换共用同一份判断。 */
#pragma once
#include <QFileInfo>
#include <QString>

namespace WorkspaceMode
{
inline QString kindOf(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "pdf")
        return "pdf";
    if (suffix == "md" || suffix == "markdown")
        return "md";
    if (suffix == "doc" || suffix == "docx")
        return "word";
    return {};
}

inline bool supports(const QString &mode, const QString &kind)
{
    return (mode == "preview" && (kind == "pdf" || kind == "md" || kind == "word")) ||
           (mode == "edit" && kind == "md") || (mode == "convert" && kind == "pdf");
}

inline QString title(const QString &mode)
{
    return mode == "edit"      ? QStringLiteral("编辑模式")
           : mode == "convert" ? QStringLiteral("转化模式")
                               : QStringLiteral("预览模式");
}
} // namespace WorkspaceMode
