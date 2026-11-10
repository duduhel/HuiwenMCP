窗口按钮直接使用 DevEco Studio 配套 OpenHarmony SDK 的原始 24×24 SVG，保留原图路径、粗细与透明度。
`file-icons.qrc` 注册 SVG，`WorkspaceTitleBar` 通过 `QIcon` 加载；不再导出 PNG 或在 C++ 中手绘图标。
鸿蒙 SVG 运行库直接保存在 `entry/libs/arm64-v8a/`，由 Hvigor 自动收集到 HAP，无需额外复制任务，详见 `third_party/qt/README.md`。

原始资源位置：previewer/common/resources/resources/base/media。
窗口 API：https://developer.huawei.com/consumer/cn/doc/harmonyos-references/arkts-apis-window-window#setwindowtitlebuttonvisible14
