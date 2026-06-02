# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

本项目自动将 Claude Code、Qwen Code、Codex App、Codex CLI 的 Windows 版本整理到同一个 GitHub Release 中，每天通过 GitHub Actions 自动发布最新组合。

## 核心架构

### 自动化流程
1. **版本检测**：从各官方源获取 Claude、Qwen、Codex App、Codex CLI 最新版本
2. **去重判断**：检查组合版本对应的 bundle Release 是否完整
3. **下载封装**：下载官方 Windows 产物，创建便携版包装
4. **发布 Release**：创建同一个 Release，并上传所有产品文件

### 关键端点
- Claude 版本 API: `https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/latest`
- Claude 下载 URL: `https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/{version}/win32-x64/claude.exe`
- Qwen 版本 API: `https://qwen-code-assets.oss-cn-hangzhou.aliyuncs.com/releases/qwen-code/latest/VERSION`
- Codex App 元数据: `https://displaycatalog.mp.microsoft.com/v7.0/products/9PLM9XGG6VKS?market=US&languages=en-US&fieldsTemplate=details`
- Codex App 离线包: Microsoft Store `msstore` 源，Product ID `9PLM9XGG6VKS`，通过 `winget download --skip-license` 下载完整 MSIX
- Codex CLI Releases: `https://github.com/openai/codex/releases`

## 文件说明

### `.github/workflows/auto-release.yml`
统一自动发布工作流，负责四个产品的同一 Release。

### `.github/workflows/check-releases.yml`
校验最新官方版本是否已发布为完整 bundle Release。

### `scripts/qwen/`
Qwen Code 版本检测、下载与便携启动脚本。

### `scripts/codex/`
Codex App 离线 MSIX 下载、Codex CLI 下载与便携启动脚本。

### `scripts/check-all-versions.ps1`
统一版本检测、bundle tag 生成和 release asset 完整性判断。

### `scripts/wrapper.bat`
便携版启动脚本，设置环境变量使 Claude Code 使用便携目录存储数据。

### `scripts/check-version.ps1`
版本检测和去重逻辑。

### `scripts/download.ps1`
下载官方 claude.exe 并进行 SHA256 校验。

## 开发注意事项

1. **工作流修改**：修改 `.github/workflows/auto-release.yml` 后需要推送到 GitHub 才能生效
2. **版本格式**：Release 标签格式为四产品组合 tag，如 `ai-tools-claude-{claude}-qwen-{qwen}-codex-app-{app}-codex-cli-{cli}`
3. **去重机制**：工作流会自动跳过资产完整的已发布组合版本
4. **测试**：可以在 GitHub Actions 页面手动触发工作流进行测试
