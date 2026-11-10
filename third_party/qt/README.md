# 项目内 Qt 开发包

当前使用 Qt 5.15.12 的 OpenHarmony ARM64 构建。运行库的唯一部署位置为
`entry/libs/arm64-v8a/`；本目录的库入口是指向该目录的相对符号链接。
头文件、官方 CMake 配置、mkspecs、宿主工具和 sdk.json 由固定依赖包恢复，不直接进入 Git。
当前支持 macOS ARM64 宿主；首次 Run 自动恢复，后续校验并复用，不重编 Qt。
包版本、下载地址、文件哈希见 `third_party/prebuilt/manifest.json`，发布及离线使用说明见
`third_party/prebuilt/README.md`。本地依赖与锁定包不一致时会停止，不覆盖自定义 Qt。

## SVG 图标运行依赖

窗口三键通过 `QIcon` 直接加载 SVG。同一套 Qt 5.15.12 SDK 的 `libQt5Svg.so` 和
`libplugins_iconengines_qsvgicon.so` 直接保存在 `entry/libs/arm64-v8a/`，并单独放行 Git 忽略规则；
其余 Qt 库与固定包逐一核对 SHA-256 一致。
Hvigor 自动将这两个库收集到 HAP，Qt 运行时发现 SVG 图标引擎，无需额外复制任务或 CMake 链接项。
其他 Qt 依赖仍由已发布固定包恢复。升级 Qt 时应同时替换这两个匹配版本的库；许可沿用配套 Qt LGPL 材料。
桌面构建使用宿主 Qt 自带的 SVG 图标插件。

维护者从原始 SDK 首次制包或升级版本时执行：

```sh
python3 third_party/qt/prepare.py \
  --source /你的/Qt5.15.12-ohos17-arm64-v8a \
  --license-source /对应的/qtbase源码目录
```

准备完成后，正常构建不再读取外部 Qt SDK 路径。moc、rcc、qmake 必须能在构建电脑
上运行（官方 Widgets 配置同时需要 uic）；换用不同宿主系统或架构时，需要重新准备匹配的工具。
修改 qt_app 或 NAPI 的 C++ 源码由应用构建增量生成新库；修改外部 Qt 内核后，先
编译 Qt SDK，再运行上述命令同步整套头文件、库和工具，避免混用不同版本。

Qt 及其依赖的许可证材料同步到应用 rawfile/third_party_licenses/qt。
分发应用仍须履行对应 LGPL 或商业许可要求；将库复制进项目不会改变许可证。
