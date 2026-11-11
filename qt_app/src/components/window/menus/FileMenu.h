/* 文件菜单只维护界面状态并发出请求，文件选择、目录打开和预览由工作区协调。 */
#pragma once
#include "SelectionMenu.h"
#include "sandbox/DocumentItem.h"
class QAction;

class FileMenu : public SelectionMenu
{
    Q_OBJECT
  public:
    explicit FileMenu(const QString &mode, QWidget *parent = nullptr);
    void setDocumentState(const DocumentItem &document, bool busy);
    void setPreviewMode(bool preview);
    bool isPreviewMode() const;
    void setMode(const QString &mode);

  signals:
    void previewModeRequested(bool preview);
    void directoryRequested(bool output);

  private:
    bool conversion_;
    bool editing_;
    QAction *placeholderAction_;
    QAction *previewAction_;
    QAction *inputDirectoryAction_;
    QAction *outputDirectoryAction_;
};
