/* 预览和转化共用工作区；模式只改变内容栏数、文档类型和菜单，窗口交接保持一致。 */
#include "ConversionWindow.h"
#include "WorkspaceMode.h"
#include "components/ConvertButton.h"
#include "components/functionBar/FileMenu.h"
#include "components/functionBar/FunctionBar.h"
#include "components/functionBar/ModeMenu.h"
#include "components/splitter/WorkspaceSplitter.h"
#include "components/window/WorkspaceTitleBar.h"
#include "dialogs/ModeSwitchDialog.h"
#include "documentView/DocumentView.h"
#include "documentView/renderers/markdown/MarkdownPreviewWidget.h"
#include "documentView/renderers/pdf/PdfPreviewWidget.h"
#include "documentView/renderers/word/WordPreviewWidget.h"
#include "sandbox/SandboxService.h"
#include "sandbox/SandboxStore.h"
#include "services/ConversionService.h"
#include "sidebars/navigation/NavigationSidebar.h"
#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QJsonArray>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#endif
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSplitterHandle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

ConversionWindow::ConversionWindow(const QString &mode, const QString &kind, QWidget *parent)
    : QWidget(parent), taskMode_(mode), taskKind_(kind), mode_(mode), kind_(kind),
      splitter_(new WorkspaceSplitter(this)), bodySplitter_(new WorkspaceSplitter(this)),
      navigationSidebar_(new NavigationSidebar(this)), functionBar_(new FunctionBar(mode, this)),
      conversion_(new ConversionService(this)), sandbox_(new SandboxService(this)),
      status_(new QLabel(this))
{
    if (taskMode_ == "convert")
        setWindowFlag(Qt::FramelessWindowHint);
    setMinimumSize(640, 480);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor("#F5F7FB"));
    colors.setColor(QPalette::Base, colors.color(QPalette::Window));
    setPalette(colors);
    setObjectName(QStringLiteral("conversionWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral("QWidget#conversionWindow, "
                                 "QWidget#documentCardBackground, QWidget#documentCardViewport "
                                 "{ background-color: %1; }")
                      .arg(colors.color(QPalette::Window).name()));
    if (taskMode_ == "convert")
    {
        // QWidget 与鸿蒙原生绘图节点分别取自己的背景角色，正文继续使用实色。
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);
        QPalette transparentColors = colors;
        transparentColors.setColor(QPalette::Window, Qt::transparent);
#ifdef Q_OS_OPENHARMONY
        transparentColors.setColor(QPalette::XComponentBackground, Qt::transparent);
#endif
        setPalette(transparentColors);
        setStyleSheet(styleSheet() +
                      QStringLiteral("QWidget#conversionWindow { background: transparent; }"));
        bodySplitter_->setPalette(colors);
        bodySplitter_->setAutoFillBackground(true);
        status_->setPalette(colors);
        status_->setAutoFillBackground(true);
    }
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    if (taskMode_ == "convert")
    {
        auto *titleBar = new WorkspaceTitleBar(this, functionBar_->commandBar());
        layout->addWidget(titleBar);
        // 复用 closeEvent，保留模式切换保护、编辑确认和首页交接流程。
        connect(titleBar, &WorkspaceTitleBar::closeRequested, this, &QWidget::close);
#ifdef Q_OS_OPENHARMONY
        connect(titleBar, &WorkspaceTitleBar::aboutDeviceRequested, &QtBridge::instance(),
                &QtBridge::requestOpenAboutDevice);
#endif
    }
    else
    {
        // 独立预览／编辑仍使用系统标题栏，菜单继续放在原功能条内。
        functionBar_->insertWidget(functionBar_->actions().last(), functionBar_->commandBar());
    }
    layout->addWidget(functionBar_);
    splitter_->installEventFilter(this);
    bodySplitter_->addWidget(navigationSidebar_);
    bodySplitter_->addWidget(splitter_);
    navigationSidebar_->hide();
    bodySplitter_->setStretchFactor(0, 0);
    bodySplitter_->setStretchFactor(1, 1);
    bodySplitter_->setSizes({0, 820});
    layout->addWidget(bodySplitter_, 1);
    status_->setContentsMargins(16, 4, 16, 4);
    status_->setMinimumHeight(28);
    status_->setWordWrap(true);
    status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(status_);
    connect(functionBar_, &FunctionBar::navigationSidebarToggled, this,
            &ConversionWindow::toggleNavigationSidebar);
    connect(bodySplitter_, &QSplitter::splitterMoved, this,
            [this]
            {
                bodySplitter_->raiseAbovePreview();
                navigationSidebar_->raiseAbovePreview();
                placeConvertButton();
            });
    connect(splitter_, &QSplitter::splitterMoved, this, [this] { placeConvertButton(); });
    connect(functionBar_, &FunctionBar::scrollSyncChanged, this,
            &ConversionWindow::refreshScrollSync);
    connect(functionBar_->modeMenu(), &ModeMenu::modeRequested, this,
            &ConversionWindow::switchMode);
    connect(functionBar_->modeMenu(), &ModeMenu::replacementRequested, this,
            &ConversionWindow::selectDocument);
    connect(functionBar_->fileMenu(), &FileMenu::previewModeRequested, this,
            [this](bool preview)
            {
                source_->setPreviewMode(preview);
                if (result_)
                    result_->setPreviewMode(preview);
                refreshScrollSync();
                placeConvertButton();
            });
    connect(functionBar_->fileMenu(), &FileMenu::directoryRequested, this,
            [this](bool output) { sandbox_->openTaskDirectory(document_, output); });
    connect(sandbox_, &SandboxService::documentSelected, this, &ConversionWindow::showDocument);
    connect(sandbox_, &SandboxService::restorationRequested, this, [this](const DocumentItem &document) {
        checkUnsaved([this, document](bool ok) {
            if (ok) restoreDocument(document);
            else sandbox_->finishRestoration(document, QStringLiteral("未替换当前编辑文档"));
        });
    });
    connect(sandbox_, &SandboxService::selectionFinished, this, [this] {
        picking_ = false;
        if (status_->text() == QStringLiteral("正在选择文件…"))
            status_->clear();
        refreshBusyState();
    });
    connect(sandbox_, &SandboxService::failed, status_, &QLabel::setText);
    connect(navigationSidebar_, &NavigationSidebar::headingActivated, this,
            [this](int line)
            {
                const QJsonObject message{
                    {"type", "navigate"}, {"line", line}, {"revision", editRevision_}};
                if (editor_)
                {
                    editor_->sendMarkdownMessage(message);
                    livePreview_->sendMarkdownMessage(message);
                }
                else if (markdownPreview_)
                    markdownPreview_->sendMarkdownMessage(message);
            });
    connect(conversion_, &ConversionService::succeeded, this,
            [this](const QString &path, const QString &message)
            {
                converting_ = false;
                retry_ = false;
                if (mode_ == "convert")
                {
                    sandbox_->markUsed(document_);
                    result_->setDocument(path, QStringLiteral("md"));
                    functionBar_->fileMenu()->setPreviewMode(true);
                    status_->setText(QStringLiteral("[code=0] %1 路径：%2").arg(message, path));
                }
                else
                {
                    source_->setDocument(path, QStringLiteral("word"),
                                         QFileInfo(workingPath_).fileName());
                    status_->setText(message);
                }
                refreshBusyState();
                if (restoringWord_)
                {
                    restoringWord_ = false;
                    sandbox_->finishRestoration(document_);
                }
            });
    connect(conversion_, &ConversionService::failed, this,
            [this](int code, const QString &message)
            {
                converting_ = false;
                retry_ = mode_ == "convert";
                const QString error = QStringLiteral("[code=%1] %2").arg(code).arg(message);
                status_->setText(error);
                if (kind_ == "word")
                {
                    source_->setDocument({}, QStringLiteral("word"));
                    conversion_->releaseDocPreview();
                }
                else
                    sandbox_->markUsed(document_);
                refreshBusyState();
                if (restoringWord_)
                {
                    restoringWord_ = false;
                    sandbox_->finishRestoration(document_, error);
                }
            });
    saveAction_ = functionBar_->addCommand(QStringLiteral("保存"));
    connect(saveAction_, &QAction::triggered, this,
            [this]
            {
                requestEditSnapshot(
                    [this](bool ok)
                    {
                        if (ok)
                            saveEdit();
                    });
            });
#ifdef Q_OS_OPENHARMONY
    connect(&QtBridge::instance(), &QtBridge::workspaceCloseCancelled, this,
            [this]
            {
                closeApproved_ = false;
                if (editor_)
                    editor_->sendMarkdownMessage({{"type", "unlock"}});
                refreshBusyState();
            });
#endif
    createDocumentViews();
    refreshBusyState();
}

// 只重建内部文档区域；主窗口、功能条、导航侧栏和任务身份一直保留。
void ConversionWindow::createDocumentViews(bool preview)
{
    layoutReady_ = false;
    editor_ = livePreview_ = markdownPreview_ = nullptr;
    if (kind_ == "word")
        source_ = new DocumentView(new WordPreviewWidget,
                                   QStringLiteral("请选择 Word（DOC / DOCX）"), true);
    else if (mode_ == "edit")
        source_ = new DocumentView(editor_ = new CefDocumentPreviewWidget(
                                       CefDocumentPreviewWidget::Format::MarkdownEditor),
                                   QStringLiteral("请选择 Markdown"), true);
    else if (kind_ == "md")
        source_ = new DocumentView(markdownPreview_ = new MarkdownPreviewWidget,
                                   QStringLiteral("请选择 Markdown"), true);
    else
        source_ = new DocumentView(new PdfPreviewWidget, QStringLiteral("请选择 PDF"), true);
    result_ = nullptr;
    button_ = nullptr;
    if (editor_)
        result_ = new DocumentView(livePreview_ = new CefDocumentPreviewWidget(
                                       CefDocumentPreviewWidget::Format::MarkdownLive),
                                   QStringLiteral("编辑后实时预览"), false);
    else if (mode_ == "convert")
    {
        result_ = new DocumentView(markdownPreview_ = new MarkdownPreviewWidget,
                                   QStringLiteral("待转化 PDF"), false);
        button_ = new ConvertButton(this);
        connect(button_, &QPushButton::clicked, this,
                [this]
                {
                    try
                    {
                        const QString input =
                            SandboxStore::validateWorkspaceFile(document_, workingPath_);
                        document_.outputPath = SandboxStore::workspaceOutputPath(document_, true);
                        if (QFileInfo::exists(document_.outputPath) ||
                            QFileInfo(document_.outputPath).isSymLink())
                            SandboxStore::validateWorkspaceFile(document_, document_.outputPath);
                        converting_ = true;
                        result_->setDocument({}, QStringLiteral("md"));
                        navigationSidebar_->setOutline({}, false);
                        status_->setText(QStringLiteral("正在转化 PDF…"));
                        sandbox_->markUsed(document_);
                        refreshBusyState();
                        conversion_->convertPdfToMd(input, document_.outputPath);
                    }
                    catch (const std::exception &error)
                    {
                        status_->setText(QString::fromUtf8(error.what()));
                    }
                });
    }
    splitter_->addWidget(source_);
    if (result_)
    {
        splitter_->addWidget(result_);
        splitter_->setSizes({600, 600});
    }
    functionBar_->setMode(mode_);
    saveAction_->setVisible(editor_ != nullptr);
    navigationSidebar_->setOutline({}, false);
    const auto refreshPreview = [this]
    {
        if (switching_)
            return;
        splitter_->raiseAbovePreview();
        bodySplitter_->raiseAbovePreview();
        navigationSidebar_->raiseAbovePreview();
        refreshScrollSync();
        placeConvertButton();
        updateEditState();
    };
    connect(source_, &DocumentView::previewReadyChanged, this, refreshPreview);
    connect(source_, &DocumentView::cardActivated, this, &ConversionWindow::selectDocument);
    connect(source_, &DocumentView::documentOpened, this,
            [this] { sandbox_->markUsed(document_); });
    if (result_)
        connect(result_, &DocumentView::previewReadyChanged, this, refreshPreview);
    if (markdownPreview_)
        connect(markdownPreview_, &CefDocumentPreviewWidget::markdownMessage, this,
                &ConversionWindow::handleOutline);
    if (editor_)
    {
        connect(editor_, &CefDocumentPreviewWidget::markdownMessage, this,
                [this](const QJsonObject &message) { handleMarkdown(message, true); });
        connect(livePreview_, &CefDocumentPreviewWidget::markdownMessage, this,
                [this](const QJsonObject &message) { handleMarkdown(message, false); });
        connect(livePreview_, &CefDocumentPreviewWidget::documentOpened, this,
                [this]
                {
                    livePreview_->sendMarkdownMessage(
                        {{"type", "content"}, {"text", editText_}, {"revision", editRevision_}});
                });
    }
    else if (result_)
    {
        connect(source_, &DocumentView::scrollPositionChanged, this,
                [this](int page, double fraction, double progress)
                {
                    if (scrollSyncEnabled())
                        result_->setScrollPosition(page, fraction, progress);
                });
        connect(result_, &DocumentView::scrollPositionChanged, this,
                [this](int page, double fraction, double progress)
                {
                    if (scrollSyncEnabled())
                        source_->setScrollPosition(page, fraction, progress);
                });
        connect(result_, &DocumentView::cardActivated, sandbox_,
                &SandboxService::openResultLocation);
    }
    source_->show();
    if (result_)
        result_->show();
    setWindowTitle(mode_ == "edit"      ? QStringLiteral("汇文MCP · Markdown 编辑")
                   : mode_ == "convert" ? QStringLiteral("汇文MCP")
                                        : QStringLiteral("汇文MCP · %1 预览")
                                              .arg(kind_ == "word" ? "Word"
                                                   : kind_ == "md" ? "Markdown"
                                                                   : "PDF"));
    functionBar_->fileMenu()->setPreviewMode(preview);
}

void ConversionWindow::switchMode(const QString &mode)
{
    if (picking_ || converting_ || switching_ || pendingSnapshot_ || closeApproved_ ||
        !ModeSwitchDialog::hasUsableFile(document_, mode))
        return;
    switching_ = true;
    refreshBusyState();
    // 模态子窗口覆盖当前预览；确认切换后再由 applyMode 释放旧视图。
    ModeSwitchDialog dialog(document_, mode, this);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    const QString path = dialog.selectedPath();
    if (!accepted)
    {
        switching_ = false;
        refreshBusyState();
        return;
    }
    checkUnsaved(
        [this, mode, path](bool ok)
        {
            if (ok)
                applyMode(mode, path, document_);
            else
            {
                switching_ = false;
                refreshBusyState();
            }
        });
}

void ConversionWindow::applyMode(const QString &mode, const QString &path,
                                 const DocumentItem &document, bool restoring)
{
    try
    {
        SandboxStore::validateWorkspaceFile(document, path);
        if (!WorkspaceMode::supports(mode, WorkspaceMode::kindOf(path)))
            throw std::runtime_error("当前文件不支持此模式");
        if (mode == "edit" && QFileInfo(path).size() > 16 * 1024 * 1024)
            throw std::runtime_error("Markdown 超过 16 MiB，无法编辑");
    }
    catch (const std::exception &error)
    {
        switching_ = false;
        status_->setText(QString::fromUtf8(error.what()));
        if (restoring)
            sandbox_->finishRestoration(document, QString::fromUtf8(error.what()));
        refreshBusyState();
        return;
    }
    const bool preview = restoring || mode != mode_ || WorkspaceMode::kindOf(path) != kind_ ||
                         functionBar_->fileMenu()->isPreviewMode();
    switching_ = true;
    refreshBusyState();
    source_->hide();
    if (result_)
        result_->hide();
    QPointer<ConversionWindow> self(this);
    // 复用已验证的 CEF 释放回执，等待旧浏览器退出后才删除旧区域；不重启 Qt／Ability。
    CefDocumentPreviewWidget::closeWorkspaceBrowsers(
        [self, mode, path, document, restoring, preview]
        {
            if (!self)
                return;
            delete self->source_;
            delete self->result_;
            delete self->button_;
            self->conversion_->releaseDocPreview();
            self->button_ = nullptr;
            self->source_ = self->result_ = nullptr;
            self->editor_ = self->livePreview_ = self->markdownPreview_ = nullptr;
            self->document_ = document;
            self->mode_ = mode;
            self->kind_ = WorkspaceMode::kindOf(path);
            self->workingPath_ = path;
            self->editText_.clear();
            self->savedText_.clear();
            self->editDirty_ = self->closeApproved_ = false;
            self->editRevision_ = 0;
            self->retry_ = false;
            self->restoringWord_ =
                restoring && self->kind_ == "word" &&
                QFileInfo(path).suffix().compare("doc", Qt::CaseInsensitive) == 0;
            self->createDocumentViews(preview);
            const bool loaded = self->loadSelectedDocument();
            self->switching_ = false;
            self->refreshBusyState();
            self->refreshScrollSync();
            // 重建可能没有改变主窗口尺寸；在本轮布局完成后同步新建的原生子节点。
            QTimer::singleShot(0, self.data(), &ConversionWindow::synchronizeLayout);
            if (restoring && !self->restoringWord_)
                self->sandbox_->finishRestoration(document,
                                                  loaded ? QString() : self->status_->text());
            else if (!restoring && loaded)
                self->sandbox_->markUsed(document);
        });
}

bool ConversionWindow::loadSelectedDocument()
{
    navigationSidebar_->setDocument(document_);
    status_->clear();
    if (kind_ == "word" &&
        QFileInfo(workingPath_).suffix().compare("doc", Qt::CaseInsensitive) == 0)
    {
        converting_ = true;
        source_->showLoading(QStringLiteral("正在转换 DOC，请稍候…"));
        status_->setText(QStringLiteral("正在转换 DOC，请稍候…"));
        conversion_->convertDocToDocx(workingPath_);
        return true;
    }
    source_->setDocument(workingPath_, kind_, QFileInfo(workingPath_).fileName());
    if (kind_ == "word")
        conversion_->releaseDocPreview();
    if (editor_)
    {
        QFile input(workingPath_);
        if (!input.open(QIODevice::ReadOnly) || input.size() > 16 * 1024 * 1024)
        {
            status_->setText(QStringLiteral("无法读取 Markdown，或文件超过 16 MiB"));
            return false;
        }
        editText_ = savedText_ =
            QString::fromUtf8(input.readAll()).replace("\r\n", "\n").replace('\r', '\n');
        result_->setDocument(workingPath_, QStringLiteral("md"),
                             QFileInfo(workingPath_).fileName());
        updateEditState();
    }
    else if (result_)
    {
        QString path;
        try
        {
            const QString output = SandboxStore::workspaceOutputPath(document_);
            if (QFileInfo::exists(output) || QFileInfo(output).isSymLink())
                path = SandboxStore::validateWorkspaceFile(document_, output);
        }
        catch (const std::exception &error)
        {
            status_->setText(QString::fromUtf8(error.what()));
        }
        result_->setDocument(path, QStringLiteral("md"));
    }
    return true;
}

// 待迁移到窗口基类：QPA 已将系统关闭转为 closeEvent，这里只通过 NAPI 发出返回首页请求。
void ConversionWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_OPENHARMONY
    event->ignore();
    if (!switching_)
        QtBridge::instance().requestReturnHome();
#else
    if (closeApproved_ || !editor_)
        QWidget::closeEvent(event);
    else
    {
        event->ignore();
        requestClosePermission([this](bool ok) { if (ok) close(); });
    }
#endif
}

void ConversionWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeConvertButton();
}

void ConversionWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &ConversionWindow::synchronizeLayout);
}

void ConversionWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (!testAttribute(Qt::WA_TranslucentBackground))
        return;
    // 当前 QPA 的 RGBX 转 RGBA 路径未清空透明像素；先清掉旧位置，再由子控件重绘。
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(event->rect(), Qt::transparent);
}

void ConversionWindow::synchronizeLayout()
{
    if (!isVisible() || switching_)
        return;
    layout()->activate();
    bodySplitter_->refresh();
    splitter_->refresh();
#ifdef Q_OS_OPENHARMONY
    // 原生子节点可能早于祖先布局创建，按最终布局同步到各自的最近原生父窗口。
    for (auto *widget : findChildren<QWidget *>())
    {
        if (widget->window() != this || !widget->isVisible())
            continue;
        auto *nativeWindow = widget->windowHandle();
        auto *nativeParent = widget->nativeParentWidget();
        if (nativeWindow && nativeParent)
            nativeWindow->setGeometry(QRect(widget->mapTo(nativeParent, QPoint()), widget->size()));
    }
#endif
    // CEF 以工作区为父节点，祖先位置变化时也要刷新浏览器的绝对区域。
    for (auto *preview : findChildren<CefDocumentPreviewWidget *>())
        preview->syncGeometry();
    splitter_->raiseAbovePreview();
    bodySplitter_->raiseAbovePreview();
    navigationSidebar_->raiseAbovePreview();
    layoutReady_ = true;
    placeConvertButton();
}

void ConversionWindow::toggleNavigationSidebar(bool visible)
{
    auto sizes = bodySplitter_->sizes();
    if (!visible && sizes[0] > 0)
        navigationSidebarWidth_ = sizes[0];
    navigationSidebar_->setVisible(visible);
    sizes[0] = visible ? std::max(navigationSidebar_->minimumWidth(), navigationSidebarWidth_) : 0;
    sizes[1] = std::max(260, bodySplitter_->width() - sizes[0] -
                                 (visible ? bodySplitter_->handleWidth() : 0));
    bodySplitter_->setSizes(sizes);
    if (visible)
        navigationSidebar_->setFocus(Qt::OtherFocusReason);
    navigationSidebar_->raiseAbovePreview();
    bodySplitter_->raiseAbovePreview();
    placeConvertButton();
}

bool ConversionWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == splitter_ && (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
                                 event->type() == QEvent::LayoutRequest))
        QTimer::singleShot(0, this, [this] { placeConvertButton(); });
    return QWidget::eventFilter(watched, event);
}

void ConversionWindow::placeConvertButton()
{
    if (!button_)
        return;
    auto *divider = splitter_->handle(1);
    if (!layoutReady_ || !isVisible() || switching_ || result_->hasDocument() || !divider ||
        !divider->isVisible() || splitter_->height() < button_->height() + 72)
    {
        button_->hide();
        return;
    }
    const QPoint center = divider->mapTo(this, divider->rect().center());
    button_->move(center.x() - button_->width() / 2,
                  splitter_->mapTo(this, QPoint(0, splitter_->height())).y() - button_->height() -
                      72);
    button_->show();
    button_->raiseAbovePreview();
}

void ConversionWindow::refreshBusyState()
{
    const bool busy = picking_ || converting_ || switching_ || pendingSnapshot_ || closeApproved_;
    if (source_)
        source_->setBusy(busy);
    functionBar_->fileMenu()->setDocumentState(document_, busy);
    functionBar_->modeMenu()->setState(mode_, document_, busy);
    if (button_)
        button_->setState(!document_.inputPath.isEmpty(), busy, retry_);
    placeConvertButton();
    updateEditState();
}

void ConversionWindow::selectDocument()
{
    if (picking_ || converting_ || switching_ || pendingSnapshot_ || closeApproved_)
        return;
    if (editor_)
    {
        checkUnsaved([this](bool ok) {
            if (!ok) return;
            picking_ = true;
            status_->setText(QStringLiteral("正在选择文件…"));
            refreshBusyState();
            sandbox_->selectDocument(taskKind_, taskMode_);
        });
        return;
    }
    picking_ = true;
    status_->setText(QStringLiteral("正在选择文件…"));
    refreshBusyState();
    sandbox_->selectDocument(taskKind_, taskMode_);
}

void ConversionWindow::showDocument(const DocumentItem &document)
{
    const QString path = taskMode_ == "edit" && !document.openPath.isEmpty() ? document.openPath
                                                                             : document.inputPath;
    applyMode(taskMode_, path, document);
}

void ConversionWindow::restoreDocument(const DocumentItem &document)
{
    if (picking_ || converting_ || switching_)
    {
        sandbox_->finishRestoration(document, QStringLiteral("工作区正在处理文件，请稍后重试"));
        return;
    }
    if (document.mode != taskMode_ || document.kind != taskKind_)
    {
        sandbox_->finishRestoration(document, QStringLiteral("任务类型与当前工作区不一致"));
        return;
    }
    const QString path = taskMode_ == "edit" && !document.openPath.isEmpty() ? document.openPath
                                                                             : document.inputPath;
    applyMode(taskMode_, path, document, true);
}

void ConversionWindow::refreshScrollSync()
{
    if (scrollSyncEnabled())
    {
        if (editor_) editor_->sendMarkdownMessage({{"type", "publish"}});
        else source_->publishScrollPosition();
    }
}

void ConversionWindow::updateEditState()
{
    if (!editor_) return;
    setWindowTitle(QStringLiteral("汇文MCP · Markdown 编辑%1").arg(editDirty_ ? " *" : ""));
    if (saveAction_)
        saveAction_->setEnabled(!document_.inputPath.isEmpty() && editor_->isPreviewReady() &&
                                !picking_ && !switching_ && !closeApproved_ && !pendingSnapshot_);
}

bool ConversionWindow::saveEdit()
{
    const QByteArray bytes = editText_.toUtf8();
    if (document_.inputPath.isEmpty() || bytes.size() > 16 * 1024 * 1024)
    {
        status_->setText(QStringLiteral("无法保存：文档未打开或内容超过 16 MiB"));
        return false;
    }
    QString target;
    try
    {
        if (WorkspaceMode::kindOf(workingPath_) != "md")
            throw std::runtime_error("当前文件不支持编辑");
        target = SandboxStore::workspaceOutputPath(document_);
        const QFileInfo output(target);
        if (output.exists() || output.isSymLink())
            SandboxStore::validateWorkspaceFile(document_, target);
        if (!editDirty_ && workingPath_ != target)
        {
            status_->setText(QStringLiteral("内容未修改，input 原件保持不变"));
            return true;
        }
        SandboxStore::workspaceOutputPath(document_, true);
    }
    catch (const std::exception &error)
    {
        status_->setText(QString::fromUtf8(error.what()));
        return false;
    }
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        status_->setText(QStringLiteral("保存失败：") + file.errorString());
        return false;
    }
    savedText_ = editText_;
    editDirty_ = false;
    document_.outputPath = target;
    workingPath_ = target;
    // Rebind resource paths without rebuilding the editors or losing their undo history.
    const QJsonObject location{{"type", "location"}, {"path", workingPath_}};
    editor_->sendMarkdownMessage(location);
    livePreview_->sendMarkdownMessage(location);
    sandbox_->markUsed(document_);
    status_->setText(QStringLiteral("已保存到 output，后续继续编辑此版本"));
    updateEditState();
    return true;
}

void ConversionWindow::requestEditSnapshot(std::function<void(bool)> completed)
{
    if (!editor_ || document_.inputPath.isEmpty()) { completed(true); return; }
    if (pendingSnapshot_ || !editor_->isPreviewReady())
    {
        status_->setText(QStringLiteral("编辑器尚未就绪，无法获取最新内容"));
        completed(false);
        return;
    }
    pendingSnapshot_ = std::move(completed);
    const int request = ++snapshotRequest_;
    editor_->sendMarkdownMessage({{"type", "snapshot"}, {"request", request}});
    refreshBusyState();
    QTimer::singleShot(5000, this, [this, request] {
        if (pendingSnapshot_ && request == snapshotRequest_)
        {
            status_->setText(QStringLiteral("读取编辑内容超时，未写入 output"));
            finishSnapshot(false);
        }
    });
}

void ConversionWindow::finishSnapshot(bool ok)
{
    if (!pendingSnapshot_) return;
    auto done = std::move(pendingSnapshot_);
    pendingSnapshot_ = {};
    done(ok);
    if (!closeApproved_ && editor_)
        editor_->sendMarkdownMessage({{"type", "unlock"}});
    refreshBusyState();
}

void ConversionWindow::checkUnsaved(std::function<void(bool)> completed)
{
    if (pendingSnapshot_) { completed(false); return; }
    if (!editor_)
    {
        completed(true);
        return;
    }
    if (editor_ && !editor_->isPreviewReady() && !editDirty_)
    {
        completed(true);
        return;
    }
    requestEditSnapshot([this, completed](bool ok) {
        if (ok && !editDirty_) { completed(true); return; }
        // Native CEF nodes sit above QWidget painting; hide them while the Qt dialog is open.
        editor_->hide();
        livePreview_->hide();
        const auto choice = QMessageBox::warning(this, QStringLiteral("未保存的修改"),
            ok ? QStringLiteral("保存当前 Markdown 的修改吗？")
               : QStringLiteral("无法获取最新编辑内容，不会保存旧缓存。放弃所有未保存修改并继续吗？"),
            (ok ? QMessageBox::Save : QMessageBox::NoButton) | QMessageBox::Discard | QMessageBox::Cancel,
            ok ? QMessageBox::Save : QMessageBox::Cancel);
        editor_->show();
        livePreview_->show();
        completed(choice == QMessageBox::Discard || (choice == QMessageBox::Save && saveEdit()));
    });
}

void ConversionWindow::requestClosePermission(std::function<void(bool)> completed)
{
    if (switching_ || pendingSnapshot_)
    {
        completed(false);
        return;
    }
    if (closeApproved_)
    {
        completed(true);
        return;
    }
    checkUnsaved([this, completed](bool ok) {
        if (ok) closeApproved_ = true;
        refreshBusyState();
        completed(ok);
    });
}

void ConversionWindow::handleMarkdown(const QJsonObject &message, bool fromEditor)
{
    const QString type = message.value("type").toString();
    if (type == "outline")
    {
        if (fromEditor)
            handleOutline(message);
        return;
    }
    const double revision = message.value("revision").toDouble(-1);
    if (!std::isfinite(revision) || revision < 0 || revision > 2147483647 ||
        std::floor(revision) != revision) return;
    if (type == "position")
    {
        const double line = message.value("line").toDouble(-1);
        const double fraction = message.value("fraction").toDouble(-1);
        const double progress = message.value("progress").toDouble(-1);
        if (scrollSyncEnabled() && revision == editRevision_ &&
            std::isfinite(line) && line >= 1 && line <= 16777217 && std::floor(line) == line &&
            std::isfinite(fraction) && fraction >= 0 && fraction <= 1 &&
            std::isfinite(progress) && progress >= 0 && progress <= 1)
            (fromEditor ? livePreview_ : editor_)->sendMarkdownMessage(message);
        return;
    }
    if (type == "saveRequest" && !fromEditor)
    {
        requestEditSnapshot([this](bool ok) { if (ok) saveEdit(); });
        return;
    }
    if (!fromEditor || revision < editRevision_) return;
    if (type == "dirty")
    {
        // This revision still has no complete text; never label the old cache with it.
        editDirty_ = true;
        updateEditState();
        return;
    }
    const bool snapshot = message.value("request").toInt(-1) == snapshotRequest_ && pendingSnapshot_;
    if (type == "tooLarge")
    {
        status_->setText(QStringLiteral("内容超过 16 MiB，未保存或更新预览；可缩减内容或放弃修改退出"));
        if (snapshot) finishSnapshot(false);
        return;
    }
    if (type != "change" && type != "save" && type != "snapshot") return;
    if (!message.value("text").isString()) return;
    const QString text = message.value("text").toString();
    if (text.toUtf8().size() > 16 * 1024 * 1024) { if (snapshot) finishSnapshot(false); return; }
    editText_ = text;
    editRevision_ = static_cast<int>(revision);
    editDirty_ = editText_ != savedText_;
    livePreview_->sendMarkdownMessage({{"type", "content"}, {"text", text}, {"revision", editRevision_}});
    if (type == "save") saveEdit();
    if (type == "snapshot" && snapshot) finishSnapshot(true);
    updateEditState();
}

void ConversionWindow::handleOutline(const QJsonObject &message)
{
    if (message.value("type").toString() != "outline" || !message.value("headings").isArray() ||
        !message.value("truncated").isBool() ||
        message.value("revision").toDouble(-1) != editRevision_)
        return;
    const auto values = message.value("headings").toArray();
    if (values.size() > 10000)
        return;
    QVector<MarkdownHeading> headings;
    headings.reserve(values.size());
    int previousLine = 0;
    for (const auto &value : values)
    {
        const auto item = value.toObject();
        const auto title = item.value("title");
        const double level = item.value("level").toDouble(-1);
        const double line = item.value("line").toDouble(-1);
        if (!title.isString() || title.toString().size() > 512 || !std::isfinite(level) ||
            level < 1 || level > 6 || std::floor(level) != level || !std::isfinite(line) ||
            line < 1 || line > 16777217 || std::floor(line) != line || line <= previousLine)
            return;
        headings.append({title.toString(), static_cast<int>(level), static_cast<int>(line)});
        previousLine = static_cast<int>(line);
    }
    navigationSidebar_->setOutline(headings, true, message.value("truncated").toBool());
}

bool ConversionWindow::scrollSyncEnabled() const
{
    // 单向滚动不向另一侧转发位置；同步滚动仍要求左右预览均已就绪。
    return result_ && functionBar_->isScrollSynchronized() && source_->isPreviewReady() &&
           result_->isPreviewReady();
}
