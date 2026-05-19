# Claude Code Windows 便携版

自动将 Claude Code 官方 Windows 版本封装为便携版，每天自动发布最新版本。

## 简介

Claude Code 是 Anthropic 官方的 Claude AI 命令行工具。官方安装版会将配置和数据存储在用户目录下，本项目将其封装为真正的便携版，所有数据都在程序目录内，无需安装，可放在 U 盘中随身携带。

## 特点

- **零安装**：无需安装，下载即用
- **便携性**：所有数据和配置都在程序目录内
- **自动更新**：每天自动检测并发布最新版本
- **原汁原味**：直接使用官方二进制，仅做便携化封装

## 下载

访问 [Releases](https://github.com/your-username/claude-code-exe/releases) 页面下载最新版本。

## 使用方法

### 方式一：使用启动脚本（推荐）

双击 `wrapper.bat` 启动，或在命令行中：

```cmd
wrapper.bat
```

### 方式二：直接运行

直接双击 `cloude.exe` 运行。

**注意**：直接运行时，配置和数据会存储在用户目录下，失去便携性。建议始终使用 `wrapper.bat` 启动。

## 文件结构

```
cloude-portable/
├── cloude.exe           # 主程序
├── wrapper.bat          # 启动脚本（推荐使用）
└── data/                # 数据目录（运行时自动创建）
    ├── .claude/         # 配置文件
    └── claude/          # 数据文件
```

## 工作原理

1. 每天自动从官方源检测最新版本
2. 下载官方 claude.exe
3. 创建便携版启动脚本 wrapper.bat
4. 发布到 GitHub Releases

## 免责声明

本项目仅对官方 Claude Code 进行便携化封装，不修改任何核心功能。

Claude Code 由 [Anthropic](https://www.anthropic.com) 开发，遵循其官方许可条款。

## 许可证

本项目与 Claude Code 官方许可保持一致。

## 链接

- [Claude Code 官网](https://claude.ai/code)
- [Claude Code 文档](https://docs.anthropic.com/en/docs/build-with-claude/claude-for-developers)
