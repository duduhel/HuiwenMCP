# cust 窗口辅助

`framelesswidget.h/.cpp` 来自本机下载的 `cust` 目录，保留 QObject 事件过滤器的组合方式。
仅转换模式创建此辅助对象，独立 PDF/Markdown 预览保留系统标题栏和三键。
由 `qt_app` 的 CMake 编译；`WorkspaceTitleBar` 将拖动助手绑定到左侧标题控件，并提供窗口控制按钮。

接入适配：移除 CEF 日志依赖；系统移动不可用时为普通窗口提供手动移动回退；
窗口隐藏或失去激活时停止拖动。关闭操作仍调用 ConversionWindow 的现有关闭流程。
未引入下载目录中独立的 CEF/JS 窗口实现，避免与项目已有浏览器生命周期重复。
