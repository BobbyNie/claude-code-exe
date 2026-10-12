# Qwen 内置 Terminal 的 PowerShell 启动链路

## 原因与决策

用户报告：使用 Terminal 项目中的 WindowsTerminal.exe，通过 PowerShell 启动时没有闪烁。
r5 已使用内置 WindowsTerminal.exe，但显式命令直接运行子启动器，没有经过 PowerShell。
默认 profile 的 commandline 被显式命令覆盖。仅修改默认 profile 不能修复启动链路。

r6 明确使用内置 WindowsTerminal.exe → 系统 Windows PowerShell → 子启动器 → Qwen。
使用系统目录的 WindowsPowerShell/v1.0/powershell.exe，不要求用户安装 PowerShell 7。
不使用系统 wt.exe，不覆盖用户已有的 Terminal 设置，不启用 screen-reader。

## 参数与数据

PowerShell 使用 -NoLogo -NoProfile -EncodedCommand。
脚本使用 UTF-16LE Base64，防止 Terminal 拆分路径中的分号。
启动器路径使用 PowerShell 单引号引用。路径中的单引号加倍。
脚本只包含启动器路径和随机管道 token，不包含用户参数、环境密钥或提示词。
原参数、项目目录和环境仍通过当前用户专用命名管道交接。
PowerShell 使用 exit $LASTEXITCODE。父启动器仍通过管道取得 Qwen 的退出码。
已有 Terminal、重定向标准流及非交互调用仍直接运行。
新安装的默认 profile 也使用 PowerShell，但运行不依赖默认 profile。

## 验证与发布

先验证旧代码不能通过 PowerShell 启动命令测试，再实现启动链路。
默认 profile 和 r6 发布标识也先运行失败测试，再修改实现。
本地 C# 测试覆盖脚本引用、启动分流、实际管道交接、超时取消和退出码。
Windows 集成测试从打包 EXE 调用生产 Launch，使用实际内置 Terminal。
测试通过 Windows 进程快照检查子启动器的父进程确实是 powershell。
测试路径包含空格、分号、中文和单引号。
工作流继续在发布前执行 Windows 测试，失败时不发布。
r6 标签、revision 资产和两处 Release 说明保持一致。
自动化测试不能证明目标机器画面不闪烁。用户仍需完成显示验收。
