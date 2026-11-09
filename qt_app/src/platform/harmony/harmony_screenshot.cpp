/* 鸿蒙截图 NAPI：异步抓取起窗前的屏幕，跨线程移交给 Qt，失败或取消时释放缓存。 */
#include "harmony_screenshot.h"
#include <hilog/log.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <window_manager/oh_display_capture.h>
#include <window_manager/oh_display_manager.h>

namespace
{
std::mutex snapshotMutex;
std::vector<PreparedScreenshot> preparedScreenshots;

struct CaptureWork
{
    napi_async_work work = nullptr;
    napi_deferred deferred = nullptr;
    std::vector<PreparedScreenshot> frames;
    std::string error;
};

// NAPI 工作线程抓取存活屏幕的 PixelMap；只写任务数据，底图由共享指针管理。
void CaptureScreens(napi_env, void *data)
{
    auto &task = *static_cast<CaptureWork *>(data);
    try
    {
        NativeDisplayManager_DisplaysInfo *rawDisplays = nullptr;
        const auto result = OH_NativeDisplayManager_CreateAllDisplays(&rawDisplays);
        const std::unique_ptr<NativeDisplayManager_DisplaysInfo,
                              decltype(&OH_NativeDisplayManager_DestroyAllDisplays)>
            displays(rawDisplays, OH_NativeDisplayManager_DestroyAllDisplays);
        if (result != DISPLAY_MANAGER_OK || !displays || !displays->displaysInfo)
            throw std::runtime_error("无法读取截图屏幕列表");
        for (uint32_t i = 0; i < displays->displaysLength; ++i)
        {
            const auto &display = displays->displaysInfo[i];
            if (!display.isAlive)
                continue;
            OH_PixelmapNative *rawPixels = nullptr;
            const auto status =
                OH_NativeDisplayManager_CaptureScreenPixelmap(display.id, &rawPixels);
            std::shared_ptr<OH_PixelmapNative> pixels(rawPixels, OH_PixelmapNative_Release);
            OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenScreenshot",
                         "HUIWEN_SCREENSHOT_PREPARE: display=%{public}u ret=%{public}d", display.id,
                         static_cast<int>(status));
            if (status == DISPLAY_MANAGER_OK && pixels)
                task.frames.push_back({display.id, std::move(pixels)});
        }
        if (task.frames.empty())
            throw std::runtime_error("系统屏幕捕获失败");
    }
    catch (const std::exception &error)
    {
        task.error = error.what();
    }
}

// 回到 ArkTS 线程：成功发布底图并兑现 Promise，失败拒绝 Promise，最后回收异步任务。
void CompleteCapture(napi_env env, napi_status status, void *data)
{
    std::unique_ptr<CaptureWork> task(static_cast<CaptureWork *>(data));
    napi_value value = nullptr;
    if (status == napi_ok && task->error.empty())
    {
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            preparedScreenshots = std::move(task->frames);
        }
        napi_get_undefined(env, &value);
        napi_resolve_deferred(env, task->deferred, value);
    }
    else
    {
        napi_value message = nullptr;
        napi_create_string_utf8(env, task->error.empty() ? "屏幕捕获未完成" : task->error.c_str(),
                                NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &value);
        napi_reject_deferred(env, task->deferred, value);
    }
    napi_delete_async_work(env, task->work);
}

// screenshotPrepareCapture：ArkTS 已获得截图权限后调用，返回
// Promise<void>；成功后才可启动截图宿主。
napi_value PrepareScreenshot(napi_env env, napi_callback_info)
{
    // 只保留本次底图；耗时系统抓屏不占用 ArkTS UI 线程。
    TakePreparedScreenshots();
    auto task = std::make_unique<CaptureWork>();
    napi_value promise = nullptr;
    if (napi_create_promise(env, &task->deferred, &promise) != napi_ok)
    {
        napi_throw_error(env, nullptr, "无法创建截图任务");
        return nullptr;
    }
    napi_value name = nullptr;
    napi_create_string_utf8(env, "HuiwenPrepareScreenshot", NAPI_AUTO_LENGTH, &name);
    auto status = napi_create_async_work(env, nullptr, name, CaptureScreens, CompleteCapture,
                                         task.get(), &task->work);
    if (status == napi_ok)
        status = napi_queue_async_work(env, task->work);
    if (status != napi_ok)
    {
        if (task->work)
            napi_delete_async_work(env, task->work);
        napi_value message = nullptr, error = nullptr;
        napi_create_string_utf8(env, "无法启动截图任务", NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &error);
        napi_reject_deferred(env, task->deferred, error);
    }
    else
    {
        task.release(); // 由 CompleteCapture 收回。
    }
    return promise;
}

// screenshotReleaseCapture：ArkTS 在失败或销毁时同步释放未交给 Qt 的底图，返回 undefined。
napi_value ClearPreparedScreenshot(napi_env env, napi_callback_info)
{
    TakePreparedScreenshots();
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}
} // namespace

// Native → Qt 的缓存交接：锁内取走全部底图，缓存立即清空；调用方接管返回值的生命周期。
std::vector<PreparedScreenshot> TakePreparedScreenshots()
{
    std::lock_guard<std::mutex> lock(snapshotMutex);
    std::vector<PreparedScreenshot> frames;
    frames.swap(preparedScreenshots);
    return frames;
}

// 导出起窗前抓屏和释放接口，与 Qt 会话桥共用同一个 Native 模块。
void InitHarmonyScreenshot(napi_env env, napi_value exports)
{
    napi_property_descriptor properties[] = {
        {"screenshotPrepareCapture", nullptr, PrepareScreenshot, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"screenshotReleaseCapture", nullptr, ClearPreparedScreenshot, nullptr, nullptr, nullptr,
         napi_default, nullptr},
    };
    if (napi_define_properties(env, exports, sizeof(properties) / sizeof(properties[0]),
                               properties) != napi_ok)
        napi_throw_error(env, nullptr, "无法导出截图接口");
}
