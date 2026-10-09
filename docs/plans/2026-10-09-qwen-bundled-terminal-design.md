# Qwen 内置 Windows Terminal

## 目标与选择

用户确认：传统 CMD 的完整 Qwen 界面会上下跳动，Windows Terminal 中没有此现象。
`--screen-reader` 可避免跳动，但用户不接受该界面。
用户选择内置官方便携 Terminal，而不是要求预先安装 Terminal，或修改 Qwen 的渲染实现。

## 封装

内置微软官方 x64 ZIP，版本固定为 1.25.2733.0。
固定版本、文件名和 SHA256 位于 `scripts/qwen/terminal-release.json`。
下载校验失败时停止构建。保留全部官方程序依赖、第三方声明和微软 MIT 许可证。
使用 ZipFile 打包，保证包含 `.portable` 文件。
首次交互启动解压到 `.qwen-terminal/terminal-<version>`。
独立 `settings` 目录不会覆盖已安装 Terminal 的设置。
重复启动复用已安装版本。修复二进制时保留用户设置。
使用安装锁避免同一目录中的并发安装。不同版本的目录不会互相删除。

## 启动与交接

传统 CMD 的常见交互模式使用内置 WindowsTerminal.exe，指定新窗口。
现有 Windows Terminal、重定向的标准流和非交互命令保持直接运行。
未知选项保持直接运行，防止新自动化参数意外打开 GUI。
正常启动不强制 screen-reader，不设置旧的擦行兼容变量。

Terminal 只收到启动器路径和随机命名管道标识。
启动器路径中的 Terminal 分号分隔符使用官方转义规则。
原始参数、项目目录和环境变量通过当前用户专用的命名管道交接。
不通过 Shell 拼接提示词。不生成含密钥或提示词的请求文件。
子启动器保留新 Terminal 的 WT_SESSION，然后在原项目目录启动 Qwen。
两种启动方式使用相同的 QWEN_HOME 和 QWEN_RUNTIME_DIR 默认值，并保留显式覆盖。
父启动器等待 Qwen 的实际退出码，不把 Terminal GUI 的退出码当作 Qwen 退出码。
Terminal 子启动器必须在 60 秒内连接。标签页关闭导致管道断开时返回 130。

## 测试与发布

按 TDD 逐项新增失败测试，再实现启动策略、请求序列化、实际子进程交接、便携解压和启动命令。
本地 .NET 8 测试运行实际子进程与管道。Windows 测试使用生产使用的 .NET Framework 编译器。
发布流程先运行原生回归测试，再构建并验证 Qwen 的 help/version。
新增真实内置 Terminal 测试，验证 WT_SESSION、项目目录、便携路径、环境变量、特殊参数和退出码。
新标签后缀为 qwen-launcher-r5，避免旧版本去重阻止发布。
所有 Windows 发布测试成功后，原有草稿上传、远端 SHA256 校验和公开发布流程才继续。

自动化测试不能证明目标机器的画面不再跳动。用户仍需对完整 Qwen 界面做最终显示验收。
系统要求：Windows 10 2004（内部版本 19041）或更新版本，或 Windows 11；x64；程序目录可写。
