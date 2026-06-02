# AI 终端工具 Windows 便携版

自动将 **Claude Code**、**Qwen Code**、**Codex App** 与 **Codex CLI** 的 Windows 版本整理到同一个 GitHub Release 中，每天通过 GitHub Actions 自动发布最新组合。

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
- **自动更新**：每天自动检测并发布最新版本
- **原汁原味**：使用官方二进制/独立包，仅做便携化封装

## 下载

在 [Releases](https://github.com/BobbyNie/claude-code-exe/releases) 页面下载最新 `AI Tools Portable Bundle`。同一个 Release asset 中包含：

- `claude.exe` / `claude-wrapper.bat`
- `qwen.exe` / `qwen-wrapper.bat`
- `Codex.msix`
- `codex.exe` / `codex-wrapper.bat`

Release 描述会列出 Claude Code、Qwen Code、Codex App、Codex CLI 各自的版本号。

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

1. 每天从官方源检测四个产品的最新版本
2. 下载 Claude/Qwen/Codex App/Codex CLI Windows 产物
3. 为 CLI 工具添加便携启动脚本
4. 将所有产物发布到同一个 GitHub Release

| 产品 | 工作流 | 版本源 | 发布产物 |
| --- | --- | --- | --- |
| Claude Code | `auto-release.yml` | Google Cloud Storage | `claude.exe` + `claude-wrapper.bat` |
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
