# AI 终端工具 Windows 便携版

自动将 **Claude Code** 与 **Qwen Code** 官方 Windows 版本封装为便携版，每天通过 GitHub Actions 自动发布最新版本。

## 简介

| 产品 | 说明 |
| --- | --- |
| [Claude Code](https://claude.ai/code) | Anthropic 官方 Claude AI 命令行工具 |
| [Qwen Code](https://qwen.ai/qwencode) | 通义千问开源终端 AI Agent |

官方安装版通常将配置和数据放在用户目录。本项目封装为便携版：所有数据在程序目录内，无需安装，可放在 U 盘中使用。

## 特点

- **零安装**：下载即用
- **便携性**：配置与运行数据集中在 `data\` 目录
- **自动更新**：每天自动检测并发布最新版本
- **原汁原味**：使用官方二进制/独立包，仅做便携化封装

## 下载

在 [Releases](https://github.com/BobbyNie/claude-code-exe/releases) 页面选择对应产品：

- **Claude Code**：标签为 `v{版本号}`，例如 `v2.1.150`
- **Qwen Code**：标签为 `qwencode-v{版本号}`，例如 `qwencode-v0.16.1`

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

解压 `qwen-code-{version}-portable.zip` 后运行：

```cmd
wrapper.bat
```

### 目录结构

```
qwen-portable/
├── wrapper.bat
├── qwen-code/          # 官方 Windows 独立运行时
│   └── bin/qwen.cmd
└── data/
    ├── .qwen/          # 配置（QWEN_HOME）
    └── qwen-runtime/   # 运行时会话数据（QWEN_RUNTIME_DIR）
```

## 工作原理

1. 每天从官方源检测最新版本
2. 下载官方 Windows 包
3. 添加便携启动脚本 `wrapper.bat`
4. 发布到 GitHub Releases

| 产品 | 工作流 | 版本源 | 发布产物 |
| --- | --- | --- | --- |
| Claude Code | `auto-release.yml` | Google Cloud Storage | `claude.exe` + `wrapper.bat` |
| Qwen Code | `auto-release-qwencode.yml` | 阿里云 OSS / GitHub Releases | 便携 zip + `wrapper.bat` |

## 免责声明

本项目仅对官方程序做便携化封装，不修改核心功能。各产品版权归其官方所有，使用须遵守相应许可条款。

## 链接

- [Claude Code 官网](https://claude.ai/code)
- [Qwen Code 官网](https://qwen.ai/qwencode)
- [Qwen Code 仓库](https://github.com/QwenLM/qwen-code)
