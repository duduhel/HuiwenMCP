/* 共用工作区主窗口：按模式组装单栏预览或双栏转化，不实现文档内部渲染。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include <QWidget>
#include <QJsonObject>
#include <functional>
class DocumentView;
class ConvertButton;
class FunctionBar;
class ConversionService;
class SandboxService;
class WorkspaceSplitter;
class QLabel;
class QAction;
class CefDocumentPreviewWidget;
class NavigationSidebar;

class ConversionWindow : public QWidget
{
  public:
    explicit ConversionWindow(const QString &mode = "convert", const QString &kind = "pdf",
                              QWidget *parent = nullptr);
    void requestClosePermission(std::function<void(bool)> completed);

  protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    void placeConvertButton();
    void createDocumentViews(bool preview = true);
    void switchMode(const QString &mode);
    void applyMode(const QString &mode, const QString &path, const DocumentItem &document,
                   bool restoring = false);
    bool loadSelectedDocument();
    void refreshBusyState();
    void refreshScrollSync();
    bool scrollSyncEnabled() const;
    void selectDocument();
    void showDocument(const DocumentItem &document);
    void restoreDocument(const DocumentItem &document);
    void handleMarkdown(const QJsonObject &message, bool fromEditor);
    void requestEditSnapshot(std::function<void(bool)> completed);
    void finishSnapshot(bool ok);
    void checkUnsaved(std::function<void(bool)> completed);
    bool saveEdit();
    void updateEditState();
    void toggleNavigationSidebar(bool visible);
    void handleOutline(const QJsonObject &message);
    // taskMode／taskKind 保留 ArkTS 会话和原任务身份，mode／kind 只控制当前视图。
    const QString taskMode_;
    const QString taskKind_;
    QString mode_;
    QString kind_;
    QString workingPath_;
    CefDocumentPreviewWidget *editor_ = nullptr;
    CefDocumentPreviewWidget *livePreview_ = nullptr;
    CefDocumentPreviewWidget *markdownPreview_ = nullptr;
    WorkspaceSplitter *splitter_;
    WorkspaceSplitter *bodySplitter_;
    NavigationSidebar *navigationSidebar_;
    int navigationSidebarWidth_ = 350;
    DocumentView *source_ = nullptr;
    DocumentView *result_ = nullptr;
    ConvertButton *button_ = nullptr;
    FunctionBar *functionBar_;
    ConversionService *conversion_ = nullptr;
    SandboxService *sandbox_;
    QLabel *status_;
    DocumentItem document_;
    bool picking_ = false;
    bool converting_ = false;
    bool restoringWord_ = false;
    bool retry_ = false;
    bool switching_ = false;
    QAction *saveAction_ = nullptr;
    QString editText_;
    QString savedText_;
    int editRevision_ = 0;
    int snapshotRequest_ = 0;
    bool editDirty_ = false;
    bool closeApproved_ = false;
    std::function<void(bool)> pendingSnapshot_;
};
