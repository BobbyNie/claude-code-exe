# AI 终端工具 Windows 便携版

自动将 **Claude Code**、**Qwen Code**、**Codex App** 与 **Codex CLI** 的 Windows 版本整理到同一个 GitHub Release 中，每周通过 GitHub Actions 自动发布最新组合。

## 简介

| 产品 | 说明 |
| --- | --- |
| [Claude Code](https://claude.ai/code) | Anthropic 官方 Claude AI 命令行工具 |
| [Qwen Code](https://qwen.ai/qwencode) | 通义千问开源终端 AI Agent |
| [Codex App](https://developers.openai.com/codex/app/windows) | OpenAI 官方 Windows 桌面应用 |
| [Codex CLI](https://github.com/openai/codex) | OpenAI 官方 Codex 命令行工具 |

官方安装版通常将配置和数据放在用户目录。本项目封装为便携版：所有数据在程序目录内，无需安装，可放在 U 盘中使用。

## 特点

- **零安装**：下载即用
- **便携性**：配置与运行数据集中在 `data\` 目录
- **自动更新**：每周自动检测并发布最新版本
- **原汁原味**：使用官方二进制/独立包，仅做便携化封装

## 下载

在 [Releases](https://github.com/BobbyNie/claude-code-exe/releases) 页面下载最新 `AI Tools Portable Bundle`。同一个 Release asset 中包含：

- `claude.exe` / `claude-wrapper.bat`
- `qwen.exe` / `qwen-wrapper.bat`
- `Codex.msix`
- `codex.exe` / `codex-wrapper.bat`

Release 描述会列出 Claude Code、Qwen Code、Codex App、Codex CLI 各自的版本号。

## ccode.exe 历史版本使用方法

自 2026-10-05 起，自动发布不再构建或上传 ccode。以下说明仅适用于历史版本或自行构建的版本。源码和独立回归测试保留。

`ccode.exe` 是 Claude Code 的单文件隔离封装。它会在首次运行时把经过校验的官方 Windows x64 payload 解压到自身目录下的 `data\cc\runtime\`，并将用户配置、缓存和临时文件隔离到 `data\cc\profile\`。请将它放在名称不含 `anthropic` 或 `claude`（不区分大小写）的目录中运行。

### 前置条件

ccode 仅支持 API 凭证模式，不支持浏览器登录、`login`、`logout` 或 `setup-token`。必须设置 `A_AUTH_TOKEN` 或 `A_API_KEY` 其中之一，并设置 `A_BASE_URL`：

| 变量 | 必填 | 说明 |
| --- | --- | --- |
| `A_AUTH_TOKEN` | 推荐 | 首选凭证变量；映射为上游 CLI 的 `ANTHROPIC_AUTH_TOKEN`。DeepSeek、阿里云百炼等 Claude Code 兼容服务应使用此变量。 |
| `A_API_KEY` | 可选 | 兼容部分服务的 API Key 变量；`A_AUTH_TOKEN` 与它任意设置一个即可。 |
| `A_BASE_URL` | 是 | API Base URL，例如 `https://api.deepseek.com/anthropic`、`http://intranet-gateway/v1` 或企业反向代理地址。ccode 不检查变量值中的保留字或协议。 |
| `A_<NAME>` | 否 | 映射给上游 CLI 所需的同名服务配置。 |
| `C_<NAME>` | 否 | 映射给上游 CLI 所需的同名 CLI 配置。 |

ccode 只允许其运行时解析 `A_BASE_URL` 中的主机；主机名本身仍不能包含保留字。可直接使用 DeepSeek 等兼容服务的 HTTP/HTTPS Base URL，或使用企业反向代理。

### 启动

#### DeepSeek

在 `cmd.exe` 中设置 DeepSeek API：

```cmd
set "A_AUTH_TOKEN=你的 DeepSeek API Key"
set "A_BASE_URL=https://api.deepseek.com/anthropic"
set "A_MODEL=deepseek-v4-pro"
set "A_DEFAULT_OPUS_MODEL=deepseek-v4-pro"
set "A_DEFAULT_SONNET_MODEL=deepseek-v4-pro"
set "A_DEFAULT_HAIKU_MODEL=deepseek-v4-flash"
set "C_SUBAGENT_MODEL=deepseek-v4-flash"
ccode.exe
```

在 PowerShell 中：

```powershell
$env:A_AUTH_TOKEN = '你的 DeepSeek API Key'
$env:A_BASE_URL = 'https://api.deepseek.com/anthropic'
$env:A_MODEL = 'deepseek-v4-pro'
$env:A_DEFAULT_OPUS_MODEL = 'deepseek-v4-pro'
$env:A_DEFAULT_SONNET_MODEL = 'deepseek-v4-pro'
$env:A_DEFAULT_HAIKU_MODEL = 'deepseek-v4-flash'
$env:C_SUBAGENT_MODEL = 'deepseek-v4-flash'
.\ccode.exe
```

DeepSeek 的 Anthropic-compatible base URL 是 `https://api.deepseek.com/anthropic`；`deepseek-v4-pro` 与 `deepseek-v4-flash` 是当前可用模型。不要留空凭证：留空会使上游 CLI 进入账号登录回退流程，进而尝试访问官方服务。

#### Qwen / 阿里云百炼

按量付费、北京地域可使用以下配置；API Key 与 Base URL 必须属于同一地域和计费方案：

```powershell
$env:A_AUTH_TOKEN = '你的百炼 API Key'
$env:A_BASE_URL = 'https://dashscope.aliyuncs.com/apps/anthropic'
$env:A_MODEL = 'qwen3.6-plus'
$env:A_DEFAULT_OPUS_MODEL = 'qwen3.6-plus'
$env:A_DEFAULT_SONNET_MODEL = 'qwen3.6-plus'
$env:A_DEFAULT_HAIKU_MODEL = 'qwen3.6-flash'
$env:C_SUBAGENT_MODEL = 'qwen3.6-plus'
.\ccode.exe
```

若使用百炼 Coding Plan，请改用其专属 endpoint `https://coding.dashscope.aliyuncs.com/apps/anthropic`、套餐专属 Key 及套餐支持的模型（例如 `qwen3.7-plus`）。新加坡、美国等地域应使用相应地域或业务空间专属 endpoint。

可像使用普通 CLI 一样传递参数，例如：

```cmd
ccode.exe --version
ccode.exe "请解释当前项目的目录结构"
```

### 隔离行为与目录结构

运行时会将以 `ANTHROPIC_` 开头的内部环境查询映射为 `A_` 前缀，将以 `CLAUDE_CODE_` 开头的内部查询映射为 `C_` 前缀；这些原始前缀不会写入 ccode 子进程环境。文件系统路径保持原样，不再对文件名做字符串替换；便携隔离由 `HOME`、`USERPROFILE`、`APPDATA`、`LOCALAPPDATA`、`TEMP` 和 `TMP` 的目录配置完成。这样 Bun 的 Win32/NT 文件查询与 Bash、搜索子进程能访问同一个实际路径。

映射时会移除完整前缀，例如 `ANTHROPIC_AUTH_TOKEN` 应配置为 `A_AUTH_TOKEN`，`CLAUDE_CODE_SUBAGENT_MODEL` 应配置为 `C_SUBAGENT_MODEL`，不要写成 `C_CODE_SUBAGENT_MODEL`。ccode 同时映射逐项查询和环境区块枚举，以兼容会在启动时一次性读取全部环境变量的 runtime。

首次启动后目录如下：

```text
ccode-portable/
├── ccode.exe
└── data/
    └── cc/
        ├── runtime/
        │   ├── aa-runtime.exe
        │   └── cc-runtime.dll
        └── profile/
            ├── home/
            ├── local/
            ├── roaming/
            └── temp/
```

请保留 `ccode.exe` 与 `data\cc\` 的相对位置；删除 `data\cc\runtime\` 后，下次启动会重新解压。单文件本身约 240 MB，首次运行后需预留额外约 250 MB 的磁盘空间给运行时和配置数据。

### 从旧版路径改写封装升级

先退出所有 ccode 会话，备份 `data\cc\profile\`，然后替换 `ccode.exe` 并重新启动。新版使用 `aa-runtime.exe`；旧的 `aa-runtime.bin` 不再使用，可以保留。不要为修复工具错误而删除整个 profile。

旧版可能将 `profile\home\.claude` 和 `.claude.json` 实际写成 `.cc` 和 `.cc.json`。新版启动时会一次性将旧 profile 中缺少的文件复制到对应的官方名称，包括 `projects` 中的历史 session；不会覆盖已有文件，也不会删除旧数据。复制保留文件时间，跳过符号链接，完成后写入 `profile\home\.cc-profile-restored-v1` 标记，避免后续启动重新恢复你主动删除的 session。若新旧两份同名 session 都存在，以当前文件为准，旧版本保留供人工核对。复制失败会停止启动并提示检查权限与磁盘空间。工作目录中的旧 `.cc` / `CC.md` 仍需人工核对；封装不会递归改名项目文件。

若 `/resume` 仍找不到历史，请核对运行的是 `ccode.exe` 还是单独安装的 `claude`，以及是否移动了 exe 却没有一起移动 `data\cc`。普通安装和便携版使用不同的用户目录。请在原来的项目目录启动；也可以用 `ccode.exe --resume <session-id>` 明确指定历史记录。外部设置的 `CLAUDE_CONFIG_DIR` 会优先于默认 profile，恢复上述目录并不能自动恢复那个自定义位置的历史。

已确认的旧版问题：部分 Win32 文件 API 改写路径，而 Bun 的其他查询及子进程使用原路径，导致第二次递归创建 `temp\claude\...\tasks` 报 `EEXIST`；将官方可执行文件改名为 `.bin` 则会令 Bun 的子进程查找报 `ENOENT`，即使文件确实存在。新版移除了文件系统 API 改写，并保留 `.exe` 扩展名。

Windows 回归测试会加载实际隔离 DLL，检查重复创建目录、文件读写/列举及跨进程访问，并从 Bun 启动官方 payload 的 `--version` 和内建 ripgrep 内容搜索/文件列举模式（`argv0="rg"`）。另用本机假 API 验证官方 `--resume` 发送的上下文包含从旧 profile 恢复的历史消息。测试安装目录含空格，无需有效 API 凭证；这不等同于真实服务端模型或交互式 `/resume` 选择器测试。

### 安全与故障排查

- ccode 显式使用 `default` 权限模式，保留本地逐次批准和拒绝机制，不随上游默认值切换为 `auto` 分类器；仍可显式选择受支持的 `plan` 或 `acceptEdits` 模式，不开放权限绕过模式。
- ccode 会拒绝同时缺少 `A_AUTH_TOKEN` 和 `A_API_KEY`、空的 `A_BASE_URL`、交互式登录命令，以及名称含保留字的启动目录或工作目录；它不会检查凭证或 URL 值中的保留字、路径或协议。
- 如果仍出现连接官方服务的提示，请确认使用的是对应历史 Release 或自行构建的 `ccode.exe`，并在同一个启动脚本中设置 `A_AUTH_TOKEN`、`A_BASE_URL` 和模型变量；`C_*` 变量不要保留 `CODE_` 片段。
- 不要使用 `setx` 持久化 API Key；优先在当前终端设置，或通过企业认可的密钥管理工具注入。
- 企业 EDR/防毒软件可能会拦截启动期相容层注入。若启动返回错误，请将 `ccode.exe` 及其解压出的 `data\cc\runtime\` 加入企业批准的白名单，而不是关闭防护软件。
- 该封装不改变官方 payload 的版权、许可或服务条款；请确认企业代理和账号使用方式符合适用条款。

## Claude Code 使用方法

### 推荐：启动脚本

```cmd
wrapper.bat
```

### 直接运行

双击 `claude.exe` 也可运行，但配置会落在用户目录，失去便携性。

### 目录结构

```
claude-portable/
├── claude.exe
├── wrapper.bat
└── data/
    ├── .claude/
    └── claude/
```

## Qwen Code 使用方法

### 推荐：启动脚本

下载 `qwen.exe` 和 `qwen-wrapper.bat` 后运行：

```cmd
qwen-wrapper.bat
```

### 目录结构

```
qwen-portable/
├── qwen.exe
├── qwen-wrapper.bat
└── data/
    ├── .qwen/          # 配置（QWEN_HOME）
    └── qwen-runtime/   # 运行时会话数据（QWEN_RUNTIME_DIR）
```

## Codex 使用方法

### Codex App

运行完整离线包安装官方 Windows 桌面应用：

```powershell
Add-AppxPackage -Path .\Codex.msix
```

Codex App 在 Microsoft Store 清单中的格式是 MSIX，不是 MSI；本项目发布的是完整离线 MSIX 包，不再发布在线安装器 stub。

### Codex CLI

推荐使用便携启动脚本：

```cmd
codex-wrapper.bat
```

也可以直接运行 `codex.exe`。便携脚本会设置 `CODEX_HOME` 到当前目录下的 `data\.codex`。

## 工作原理

1. 每周从官方源检测四个产品的最新版本
2. 下载 Claude/Qwen/Codex App/Codex CLI Windows 产物
3. 为 CLI 工具添加便携启动脚本
4. 为四个产品和便携脚本生成 SHA256 校验清单
5. 按四个上游版本号生成组合标签；公开 Release 已存在且包含四个产品、校验清单及所有必需附件时跳过
6. 有更新时将四个产品及便携脚本发布到同一个 GitHub Release；附件不完整时修复同一个 Release，不删除标签

每周发布工作流在 `main` 上运行，计划时间为每周一 UTC 00:00（澳门/北京时间 08:00）；
GitHub 排队可能使实际执行时间延后。发布流程成功完成后自动核对已发布附件，并保留每周一 UTC 01:00 的定时检查和手动检查；不在推送时抢先检查尚未发布的附件。
也可以在 Actions 页面手动运行 `Auto Release AI Tools Portable`。

自动发布仅包含 Claude Code、Qwen Code、Codex App 和 Codex CLI。
自动发布不再构建、验收或上传 `ccode.exe` 与 `ccode-provenance.json`，也不再要求这两个附件。
新 Release 先以草稿上传，核对远端附件大小和 SHA256 后公开；失败不会报告发布成功。
历史 Release 中的 ccode 附件保留，不自动删除。
`main` 上封装代码或测试变更会触发 Windows x64 回归测试；不使用企业签名材料作为普通版门槛。

| 产品 | 工作流 | 版本源 | 发布产物 |
| --- | --- | --- | --- |
| Claude Code | `auto-release.yml` | Google Cloud Storage | `claude.exe` + `claude-wrapper.bat` |
| ccode 历史版本／自行构建 | 不再自动发布 | 保留源码与独立回归测试 | 不上传至新 Release |
| Qwen Code | `auto-release.yml` | 阿里云 OSS | `qwen.exe` + `qwen-wrapper.bat` |
| Codex App | `auto-release.yml` | Microsoft Store metadata / Microsoft CDN | `Codex.msix` |
| Codex CLI | `auto-release.yml` | `openai/codex` GitHub Releases | `codex.exe` + `codex-wrapper.bat` |

## 免责声明

本项目仅对官方程序做便携化封装，不修改核心功能。各产品版权归其官方所有，使用须遵守相应许可条款。

## 链接

- [Claude Code 官网](https://claude.ai/code)
- [Qwen Code 官网](https://qwen.ai/qwencode)
- [Qwen Code 仓库](https://github.com/QwenLM/qwen-code)
- [Codex Windows App 文档](https://developers.openai.com/codex/app/windows)
- [Codex CLI 仓库](https://github.com/openai/codex)

Windows 回归在每次运行开始时读取一次 Claude Code 官方 `latest` 和 `stable` 版本，所有构建、离线及工作区测试共用该快照，不再锁定历史版本。两个通道不同时验证升级和回滚；相同时仅构建一次并跳过跨版本任务，不将同版本测试当作跨版本通过。此独立回归流程不会将 ccode 发布到 Release。
