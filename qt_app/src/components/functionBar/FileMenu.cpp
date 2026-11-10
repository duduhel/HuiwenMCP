/* 文件菜单仅管理查看方式与目录；业务模式及更换文件入口由独立 ModeMenu 管理。 */
#include "FileMenu.h"
#include <QAction>
#include <QActionGroup>
#include <QFileInfo>

FileMenu::FileMenu(const QString &mode, QWidget *parent)
    : SelectionMenu(parent), conversion_(mode == "convert"), editing_(mode == "edit")
{
    auto *viewMenu = new SelectionMenu(this);
    viewMenu->setTitle(QStringLiteral("查看模式"));
    addMenu(viewMenu);
    auto *views = new QActionGroup(viewMenu);
    views->setExclusive(true);
    placeholderAction_ = viewMenu->addAction(QStringLiteral("占位图模式"));
    previewAction_ = viewMenu->addAction(QStringLiteral("预览模式"));
    for (auto *action : {placeholderAction_, previewAction_})
    {
        action->setCheckable(true);
        views->addAction(action);
    }
    previewAction_->setChecked(true);
    if (editing_)
    {
        previewAction_->setText(QStringLiteral("编辑与预览"));
    }
    connect(placeholderAction_, &QAction::triggered, this, [this] { setPreviewMode(false); });
    connect(previewAction_, &QAction::triggered, this, [this] { setPreviewMode(true); });

    auto *directoryMenu = new SelectionMenu(this);
    directoryMenu->setTitle(QStringLiteral("文件目录"));
    addMenu(directoryMenu);
    inputDirectoryAction_ = directoryMenu->addAction((conversion_ || editing_) ? QStringLiteral("输入目录")
                                                                 : QStringLiteral("所在目录"));
    outputDirectoryAction_ = directoryMenu->addAction(QStringLiteral("输出目录"));
    outputDirectoryAction_->setVisible(conversion_ || editing_);
    connect(inputDirectoryAction_, &QAction::triggered, this,
            [this] { emit directoryRequested(false); });
    connect(outputDirectoryAction_, &QAction::triggered, this,
            [this] { emit directoryRequested(true); });

    setDocumentState({}, false);
}

void FileMenu::setMode(const QString &mode)
{
    conversion_ = mode == "convert";
    editing_ = mode == "edit";
    // 编辑视图直接对应业务编辑模式，不再留下第二个未实现的编辑入口。
    previewAction_->setText(editing_ ? QStringLiteral("编辑与预览") : QStringLiteral("预览模式"));
}

// 菜单点击与程序切换共用入口，只改变查看方式，不改变业务模式。
void FileMenu::setPreviewMode(bool preview)
{
    if (editing_) preview = true;
    (preview ? previewAction_ : placeholderAction_)->setChecked(true);
    emit previewModeRequested(preview);
}

bool FileMenu::isPreviewMode() const
{
    return previewAction_->isChecked();
}

void FileMenu::setDocumentState(const DocumentItem &document, bool busy)
{
    const bool hasDocument = !document.inputPath.isEmpty();
    const bool hasOutput = hasDocument && (document.mode != "preview" || conversion_ || editing_ ||
                                           QFileInfo(document.taskPath + "/output").isDir());
    inputDirectoryAction_->setText(document.mode == "preview" ? QStringLiteral("所在目录")
                                                              : QStringLiteral("输入目录"));
    outputDirectoryAction_->setVisible(hasOutput);
    placeholderAction_->setEnabled(!busy && !editing_);
    previewAction_->setEnabled(hasDocument && !busy);
    inputDirectoryAction_->setEnabled(hasDocument);
    outputDirectoryAction_->setEnabled(hasOutput);
}
