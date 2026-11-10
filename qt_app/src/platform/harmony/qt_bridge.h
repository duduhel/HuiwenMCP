/* 鸿蒙会话桥：具名业务 API 经 TSFN 发往 ArkTS，结果排队送回 Qt 线程。 */
#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <napi/native_api.h>

class QtBridge : public QObject
{
    Q_OBJECT
  public:
    // Qt 业务侧共用的桥对象，信号在 Qt 主线程派发。
    static QtBridge &instance();
    // 读取当前有效会话 ID；0 表示尚未注册或已解除，供异步操作校验归属。
    static quint64 workspaceId();
    // 读取注册时发布的 mode／kind／截图缓存目录，供 main 组装对应窗口。
    static QJsonObject workspaceOptions();

    // Qt → ArkTS：按 kind／mode 打开系统选择器；结果经 documentSelectionCompleted 返回。
    bool requestDocumentSelection(const QString &kind, const QString &mode);
    // Qt → ArkTS：校验并定位 path 指向的输出文件；请求结果经 directoryOpenCompleted 返回。
    bool requestOpenOutputLocation(const QString &path);
    // Qt → ArkTS：校验并打开 path 指向的任务目录；请求结果经 directoryOpenCompleted 返回。
    bool requestOpenTaskDirectory(const QString &path);
    // Qt → ArkTS：打开系统设置的“关于本机”，与首页鸿蒙字标行为一致。
    bool requestOpenAboutDevice();
    // Qt → ArkTS：反馈 taskPath 的实际恢复结果，error 为空表示加载及使用时间更新成功。
    bool notifyWorkspaceRestoreResult(const QString &taskPath, const QString &error);
    // Qt → ArkTS：窗口已曝光，可以结束原首页；此通知不等待文件恢复。
    bool notifyWorkspaceReady();
    // Qt → ArkTS：用户请求关闭工作区；Ability 决定返回首页或退出应用。
    bool requestReturnHome();
    // Qt → ArkTS：编辑器的关闭确认结果，approved 为 false 时保留当前窗口。
    bool notifyWorkspaceCloseApproval(bool approved);
    // Qt → ArkTS：工作区已隐藏且 CEF 已释放，允许 Ability 执行最终关闭。
    bool notifyWorkspaceClosed();
    // Qt → ArkTS：报告截图结束、结果路径和错误；cached 表示结果已缓存在截图目录。
    bool notifyScreenshotFinished(bool ok, const QString &path, const QString &error, bool cached);
    // Qt → ArkTS：把缓存 path 保存到授权目录；error 为预先保存失败原因，displayId 用于屏幕提示。
    bool requestScreenshotDownload(const QString &path, const QString &error, quint64 displayId);
    // Qt → ArkTS：颜色已复制；format／value 为格式及文本，color 为 HEX 色块，displayId 用于提示。
    bool notifyColorCopied(const QString &format, const QString &value, const QString &color,
                           quint64 displayId);
  signals:
    // 注册或解除会话后通知 Qt 重建／释放对应窗口，不直接结束应用进程。
    void workspaceChanged();
    // ArkTS → Qt：首页已就绪或用户选择退出，开始隐藏窗口并释放 CEF。
    void closeRequested();
    // ArkTS → Qt：请求编辑器检查保存状态，随后回报关闭确认结果。
    void closeApprovalRequested();
    // ArkTS → Qt：首页启动失败，撤销编辑器的关闭状态。
    void workspaceCloseCancelled();
    // ArkTS → Qt：系统选择结果；result 为沙箱文档及 error，取消时没有文档字段。
    void documentSelectionCompleted(const QJsonObject &result);
    // ArkTS → Qt：请求加载已校验任务，完成后由 Qt 回报任务恢复结果。
    void workspaceRestorationRequested(const QJsonObject &result);
    // ArkTS → Qt：目录定位请求的结果，error 为空表示系统接受请求。
    void directoryOpenCompleted(const QString &error);
    // ArkTS → Qt：下载完成回包，path 为目标文件，error 为空时才按成功处理。
    void screenshotDownloadCompleted(const QString &path, const QString &error);

  private:
    // Qt → TSFN 的唯一发送入口；所有 bool 方法只返回是否入队，不代表业务已成功。
    bool post(const QString &action, QJsonObject parameters = {});
};

// 模块初始化时导出 sessionAttach／sessionDetach／sessionDeliverResult，供 ArkTS 会话桥调用。
void InitQtBridge(napi_env env, napi_value exports);
