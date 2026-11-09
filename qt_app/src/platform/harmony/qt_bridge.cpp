/* 鸿蒙会话 NAPI：协议仅在此解析；会话 ID 隔离旧宿主回包、排队事件和延迟注销。 */
#include "qt_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <cmath>
#include <memory>
#include <mutex>

namespace
{
std::mutex callbackMutex;
napi_threadsafe_function callback = nullptr;
quint64 generation = 0;
QJsonObject options;

struct ArkTsEvent
{
    quint64 sessionId;
    QByteArray json;
};

QString ReadOption(napi_env env, napi_value value, size_t limit = 16)
{
    size_t size = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &size) != napi_ok || size > limit)
        return {};
    QByteArray text(static_cast<int>(size + 1), '\0');
    if (napi_get_value_string_utf8(env, value, text.data(), text.size(), &size) != napi_ok)
        return {};
    return QString::fromUtf8(text.constData(), static_cast<int>(size));
}

bool ReadSessionId(napi_env env, napi_value value, quint64 &id)
{
    double number = 0;
    if (napi_get_value_double(env, value, &number) != napi_ok || !std::isfinite(number) ||
        number < 1 || number > 9007199254740991.0 || std::floor(number) != number)
        return false;
    id = static_cast<quint64>(number);
    return true;
}

// 冷启动由 main 读取，重入时排队通知 Qt；不改变 QPA 宿主交接顺序。
void NotifyWorkspaceChanged()
{
    if (auto *app = QCoreApplication::instance())
        QMetaObject::invokeMethod(
            app, [] { emit QtBridge::instance().workspaceChanged(); }, Qt::QueuedConnection);
}

// TSFN 在 ArkTS 线程消费事件；过期会话不投递，unique_ptr 在正常投递或队列中止时回收数据。
void CallArkTs(napi_env env, napi_value fn, void *, void *data)
{
    std::unique_ptr<ArkTsEvent> event(static_cast<ArkTsEvent *>(data));
    if (!env || !fn || event->sessionId != QtBridge::workspaceId())
        return;
    napi_value argument, receiver;
    if (napi_create_string_utf8(env, event->json.constData(), event->json.size(), &argument) !=
            napi_ok ||
        napi_get_undefined(env, &receiver) != napi_ok)
        return;
    if (napi_call_function(env, receiver, fn, 1, &argument, nullptr) != napi_ok)
    {
        qWarning() << "HUIWEN_BRIDGE_CALLBACK_FAILED session=" << event->sessionId;
        bool pending = false;
        if (napi_is_exception_pending(env, &pending) == napi_ok && pending)
        {
            napi_value exception;
            napi_get_and_clear_last_exception(env, &exception);
        }
    }
}

// ArkTS → Native：按参数 sessionId 注销回调，过期 ID 忽略；返回 undefined，不关闭窗口或进程。
napi_value SessionDetach(napi_env env, napi_callback_info info)
{
    size_t count = 1;
    napi_value value;
    quint64 id = 0;
    if (napi_get_cb_info(env, info, &count, &value, nullptr, nullptr) != napi_ok || count != 1 ||
        !ReadSessionId(env, value, id))
    {
        napi_throw_type_error(env, nullptr, "注销需要有效的会话 ID");
        return nullptr;
    }
    bool detached = false;
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (callback && id == generation)
        {
            napi_release_threadsafe_function(callback, napi_tsfn_abort);
            callback = nullptr;
            options = {};
            detached = true;
        }
    }
    if (detached)
        NotifyWorkspaceChanged();
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

// ArkTS → Native：接收 callback、mode、kind 和可选缓存目录；替换旧回调并返回新会话 ID。
// 先创建成功再发布；回调仅经 TSFN 执行，不保存 napi_env 供 Qt 线程直接调用。
napi_value SessionAttach(napi_env env, napi_callback_info info)
{
    size_t count = 4;
    napi_value arguments[4], name;
    napi_valuetype type;
    if (napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr) != napi_ok || count < 3 ||
        napi_typeof(env, arguments[0], &type) != napi_ok || type != napi_function)
    {
        napi_throw_type_error(env, nullptr, "需要会话回调、业务模式和文档类型");
        return nullptr;
    }
    const QString mode = ReadOption(env, arguments[1]);
    const QString kind = ReadOption(env, arguments[2]);
    const QString cacheRoot = count >= 4 ? ReadOption(env, arguments[3], 4096) : QString();
    if (!((mode == "preview" && (kind == "pdf" || kind == "md" || kind == "word")) ||
          (mode == "edit" && kind == "md") || (mode == "convert" && kind == "pdf") ||
          (mode == "screenshot" && kind.isEmpty() && cacheRoot.startsWith('/'))))
    {
        napi_throw_type_error(env, nullptr, "Qt 会话仅支持文档预览、MD 编辑、PDF 转 MD 和截图");
        return nullptr;
    }
    napi_threadsafe_function next = nullptr;
    if (napi_create_string_utf8(env, "HuiwenQtSession", NAPI_AUTO_LENGTH, &name) != napi_ok ||
        napi_create_threadsafe_function(env, arguments[0], nullptr, name, 0, 1, nullptr, nullptr,
                                        nullptr, CallArkTs, &next) != napi_ok)
    {
        napi_throw_error(env, nullptr, "无法注册 Qt 会话回调");
        return nullptr;
    }
    if (napi_unref_threadsafe_function(env, next) != napi_ok)
    {
        napi_release_threadsafe_function(next, napi_tsfn_abort);
        napi_throw_error(env, nullptr, "无法配置 Qt 会话回调");
        return nullptr;
    }
    napi_value result;
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (napi_create_double(env, static_cast<double>(generation + 1), &result) != napi_ok)
        {
            napi_release_threadsafe_function(next, napi_tsfn_abort);
            napi_throw_error(env, nullptr, "无法创建 Qt 会话 ID");
            return nullptr;
        }
        if (callback)
            napi_release_threadsafe_function(callback, napi_tsfn_abort);
        callback = next;
        options = QJsonObject{{"mode", mode}, {"kind", kind}, {"screenshotCacheRoot", cacheRoot}};
        ++generation;
    }
    NotifyWorkspaceChanged();
    return result;
}

bool ValidDocumentResult(const QJsonObject &result)
{
    if (!result.contains("document"))
        return true; // 取消选取或错误回包不带文档。
    if (!result.value("document").isObject())
        return false;
    const auto document = result.value("document").toObject();
    for (const auto *key :
         {"name", "kind", "mode", "taskPath", "inputPath", "outputPath", "openPath"})
        if (!document.value(key).isString())
            return false;
    return document.value("lastUsedAt").isDouble();
}

// ArkTS → Native：校验 sessionId 与 JSON 结果，返回 undefined；QueuedConnection 到 Qt 后再次核对
// ID。
napi_value SessionDeliverResult(napi_env env, napi_callback_info info)
{
    size_t count = 2;
    napi_value arguments[2];
    quint64 id = 0;
    if (napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr) != napi_ok || count != 2 ||
        !ReadSessionId(env, arguments[0], id))
    {
        napi_throw_type_error(env, nullptr, "回包需要会话 ID 和 JSON 结果");
        return nullptr;
    }
    const QString json = ReadOption(env, arguments[1], 32768);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json.toUtf8(), &error);
    const auto result = document.object();
    const QString action = result.value("action").toString();
    const bool documentResult = action == "pick" || action == "restore";
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        !(documentResult || action == "open" || action == "openDirectory" ||
          action == "closeAborted" || action == "downloadScreenshot" || action == "confirmClose" ||
          action == "close") ||
        (result.contains("error") && !result.value("error").isString()) ||
        (documentResult && !ValidDocumentResult(result)) ||
        (action == "downloadScreenshot" && !result.value("path").isString()))
    {
        napi_throw_type_error(env, nullptr, "Qt 会话结果协议无效");
        return nullptr;
    }
    if (id == QtBridge::workspaceId())
        if (auto *app = QCoreApplication::instance())
            QMetaObject::invokeMethod(
                app,
                [result, action, id]
                {
                    if (id != QtBridge::workspaceId())
                        return;
                    auto &bridge = QtBridge::instance();
                    if (action == "confirmClose")
                        emit bridge.closeApprovalRequested();
                    else if (action == "close")
                        emit bridge.closeRequested();
                    else if (action == "closeAborted")
                        emit bridge.workspaceCloseCancelled();
                    else if (action == "downloadScreenshot")
                        emit bridge.screenshotDownloadCompleted(result.value("path").toString(),
                                                                result.value("error").toString());
                    else if (action == "pick" || action == "restore")
                    {
                        auto document = result.value("document").toObject();
                        document.insert("error", result.value("error").toString());
                        if (action == "pick")
                            emit bridge.documentSelectionCompleted(document);
                        else
                            emit bridge.workspaceRestorationRequested(document);
                    }
                    else
                        emit bridge.directoryOpenCompleted(result.value("error").toString());
                },
                Qt::QueuedConnection);
    napi_value value;
    napi_get_undefined(env, &value);
    return value;
}
} // namespace

QtBridge &QtBridge::instance()
{
    static QtBridge bridge;
    return bridge;
}

quint64 QtBridge::workspaceId()
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    return callback ? generation : 0;
}

QJsonObject QtBridge::workspaceOptions()
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    return options;
}

// Qt 线程组装协议并携带当前会话 ID 入队；成功后数据所有权转交 TSFN，失败仍由本地释放。
bool QtBridge::post(const QString &action, QJsonObject parameters)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (!callback)
        return false;
    parameters.insert("action", action);
    parameters.insert("sessionId", static_cast<double>(generation));
    auto event = std::make_unique<ArkTsEvent>(
        ArkTsEvent{generation, QJsonDocument(parameters).toJson(QJsonDocument::Compact)});
    if (napi_call_threadsafe_function(callback, event.get(), napi_tsfn_nonblocking) != napi_ok)
        return false;
    event.release();
    return true;
}

bool QtBridge::requestDocumentSelection(const QString &kind, const QString &mode)
{
    return post("pick", {{"kind", kind}, {"mode", mode}});
}
bool QtBridge::requestOpenOutputLocation(const QString &path)
{
    return post("open", {{"path", path}});
}
bool QtBridge::requestOpenTaskDirectory(const QString &path)
{
    return post("openDirectory", {{"path", path}});
}
bool QtBridge::notifyWorkspaceRestoreResult(const QString &taskPath, const QString &error)
{
    return post("restoreResult", {{"taskPath", taskPath}, {"error", error}});
}
bool QtBridge::notifyWorkspaceReady()
{
    return post("ready");
}
bool QtBridge::requestReturnHome()
{
    return post("close");
}
bool QtBridge::notifyWorkspaceCloseApproval(bool approved)
{
    return post(approved ? "closeApproved" : "closeCancelled");
}
bool QtBridge::notifyWorkspaceClosed()
{
    return post("closed");
}
bool QtBridge::notifyScreenshotFinished(bool ok, const QString &path, const QString &error,
                                        bool cached)
{
    return post("screenshotFinished",
                {{"ok", ok}, {"path", path}, {"error", error}, {"cached", cached}});
}
bool QtBridge::requestScreenshotDownload(const QString &path, const QString &error,
                                         quint64 displayId)
{
    return post("downloadScreenshot",
                {{"path", path}, {"error", error}, {"displayId", static_cast<double>(displayId)}});
}
bool QtBridge::notifyColorCopied(const QString &format, const QString &value, const QString &color,
                                 quint64 displayId)
{
    return post("colorCopied", {{"format", format},
                                {"value", value},
                                {"color", color},
                                {"displayId", static_cast<double>(displayId)}});
}

// 与 Python、沙箱、截图共用 libhuiwen_native.so；此处只导出会话传输接口，业务方法留在 QtBridge。
void InitQtBridge(napi_env env, napi_value exports)
{
    napi_property_descriptor methods[] = {
        {"sessionAttach", nullptr, SessionAttach, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionDetach", nullptr, SessionDetach, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionDeliverResult", nullptr, SessionDeliverResult, nullptr, nullptr, nullptr,
         napi_default, nullptr},
    };
    if (napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods) !=
        napi_ok)
        napi_throw_error(env, nullptr, "无法导出 Qt 会话接口");
}
