/* 沙箱服务实现：鸿蒙异步请求 ArkTS；桌面使用 Qt 选择器及应用数据目录。 */
#include "SandboxService.h"
#include "SandboxStore.h"
#include <QDateTime>
#include <QDebug>
#include <exception>
#ifdef Q_OS_OPENHARMONY
#include <QJsonObject>
#include "platform/harmony/qt_bridge.h"
#else
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSaveFile>
#include <QUrl>
#include <stdexcept>
#endif

SandboxService::SandboxService(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_OPENHARMONY
    const auto receiveDocument = [this](const QJsonObject &result, bool restore)
    {
        if (!result.value("inputPath").toString().isEmpty())
        {
            const DocumentItem document{result.value("kind").toString(),
                                        result.value("name").toString(),
                                        result.value("inputPath").toString(),
                                        result.value("outputPath").toString(),
                                        result.value("taskPath").toString(),
                                        result.value("mode").toString(),
                                        static_cast<qint64>(result.value("lastUsedAt").toDouble()),
                                        result.value("openPath").toString()};
            if (restore)
                emit restorationRequested(document);
            else
                emit documentSelected(document);
        }
        if (!restore)
            emit selectionFinished();
        if (!result.value("error").toString().isEmpty())
            emit failed(result.value("error").toString());
    };
    auto &bridge = QtBridge::instance();
    connect(&bridge, &QtBridge::documentSelectionCompleted, this,
            [receiveDocument](const QJsonObject &result) { receiveDocument(result, false); });
    connect(&bridge, &QtBridge::workspaceRestorationRequested, this,
            [receiveDocument](const QJsonObject &result) { receiveDocument(result, true); });
    connect(&bridge, &QtBridge::directoryOpenCompleted, this,
            [this](const QString &error)
            {
                if (!error.isEmpty())
                    emit failed(error);
            });
#endif
}

void SandboxService::selectDocument(const QString &kind, const QString &mode)
{
#ifdef Q_OS_OPENHARMONY
    if (!QtBridge::instance().requestDocumentSelection(kind, mode))
    {
        emit selectionFinished();
        emit failed(QStringLiteral("鸿蒙文件选择接口尚未就绪"));
    }
#else
    const bool markdown = kind == "md";
    const bool word = kind == "word";
    const QString file =
        QFileDialog::getOpenFileName(nullptr,
                                     word       ? QStringLiteral("请选择 Word（DOC / DOCX）")
                                     : markdown ? QStringLiteral("请选择 Markdown")
                                                : QStringLiteral("请选择 PDF"),
                                     {},
                                     word       ? "Word (*.doc *.docx)"
                                     : markdown ? "Markdown (*.md *.markdown)"
                                                : "PDF (*.pdf)");
    if (!file.isEmpty())
    {
        DocumentItem document;
        try
        {
            const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            document = SandboxStore::createDocument(root, QFileInfo(file).fileName(), kind, mode);
            QFile source(file);
            QSaveFile target(document.inputPath);
            if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly))
                throw std::runtime_error("无法打开文档或沙箱副本");
            while (!source.atEnd())
            {
                const auto bytes = source.read(64 * 1024);
                if (source.error() != QFileDevice::NoError || target.write(bytes) != bytes.size())
                    throw std::runtime_error("复制文档到应用数据目录失败");
            }
            if (!target.commit())
                throw std::runtime_error("保存沙箱文档失败");
            markUsed(document);
            document.lastUsedAt = QFileInfo(document.taskPath).lastModified().toMSecsSinceEpoch();
            emit documentSelected(document);
        }
        catch (const std::exception &error)
        {
            if (!document.taskPath.isEmpty())
                SandboxStore::discardDocument(document);
            emit failed(QString::fromUtf8(error.what()));
        }
    }
    emit selectionFinished();
#endif
}

void SandboxService::markUsed(const DocumentItem &document)
{
    try
    {
        SandboxStore::markUsed(document);
    }
    catch (const std::exception &error)
    {
        // 时间更新失败不能把已完成的转换改判失败，保留可定位日志。
        qWarning().noquote() << "HUIWEN_SANDBOX stage=touch_failed task=" << document.taskPath
                             << error.what();
    }
}

void SandboxService::finishRestoration(const DocumentItem &document, const QString &error)
{
    QString message = error;
    if (message.isEmpty())
    {
        try
        {
            SandboxStore::markUsed(document);
        }
        catch (const std::exception &failure)
        {
            message = QString::fromUtf8(failure.what());
        }
    }
#ifdef Q_OS_OPENHARMONY
    if (!QtBridge::instance().notifyWorkspaceRestoreResult(document.taskPath, message))
        message = QStringLiteral("无法通知首页任务恢复结果");
#endif
    if (!message.isEmpty())
        emit failed(message);
}

void SandboxService::openResultLocation(const QString &path)
{
#ifdef Q_OS_OPENHARMONY
    qInfo().noquote() << "HUIWEN_OUTPUT stage=locate_start platform=harmony path=" << path;
    if (!QtBridge::instance().requestOpenOutputLocation(path))
    {
        qWarning().noquote() << "HUIWEN_OUTPUT stage=locate_failed reason=bridge_unavailable";
        emit failed(QStringLiteral("鸿蒙输出位置接口尚未就绪"));
    }
    else
        qInfo().noquote() << "HUIWEN_OUTPUT stage=locate_requested platform=harmony path=" << path;
#else
    const QFileInfo output(path);
    if (!output.isAbsolute() ||
        output.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) != 0 ||
        !output.isFile() || !output.isReadable() || output.size() <= 0)
    {
        emit failed(QStringLiteral("Markdown 文件不存在、为空或不可读取：") + path);
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(output.absolutePath())))
        emit failed(QStringLiteral("无法打开输出目录"));
#endif
}

void SandboxService::openTaskDirectory(const DocumentItem &document, bool output)
{
    if (document.taskPath.isEmpty() || document.inputPath.isEmpty())
    {
        emit failed(QStringLiteral("请先选择文档"));
        return;
    }
    const QString path = document.mode == "preview"
                             ? document.taskPath
                             : document.taskPath + (output ? "/output" : "/input");
#ifdef Q_OS_OPENHARMONY
    // ArkTS 使用自己的 filesDir 再经 NAPI 校验目录归属后调用系统文件管理器。
    if (!QtBridge::instance().requestOpenTaskDirectory(path))
        emit failed(QStringLiteral("鸿蒙任务目录接口尚未就绪"));
#else
    try
    {
        const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const QString directory = SandboxStore::validateTaskDirectory(root, path);
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory)))
            emit failed(QStringLiteral("无法打开任务目录"));
    }
    catch (const std::exception &error)
    {
        emit failed(QString::fromUtf8(error.what()));
    }
#endif
}
