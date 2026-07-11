# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## 项目概述

本项目自动将 Codex 官方 Windows 版本封装为便携版，每天通过 GitHub Actions 自动发布最新版本。

## 核心架构

### 自动化流程
1. **版本检测**：从 Google Cloud Storage API 获取最新版本号
2. **去重判断**：检查 GitHub Releases 是否已存在该版本
3. **下载封装**：下载官方 Codex.exe，创建便携版包装
4. **发布 Release**：创建新 Release 并上传文件

### 关键端点
- 版本 API: `https://storage.googleapis.com/Codex-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/Codex-releases/latest`
- 下载 URL: `https://storage.googleapis.com/Codex-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/Codex-releases/{version}/win32-x64/Codex.exe`

## 文件说明

### `.github/workflows/auto-release.yml`
GitHub Actions 主工作流，包含完整的自动化逻辑。

### `scripts/wrapper.bat`
便携版启动脚本，设置环境变量使 Codex 使用便携目录存储数据。

### `scripts/check-version.ps1`
版本检测和去重逻辑。

### `scripts/download.ps1`
下载官方 Codex.exe 并进行 SHA256 校验。

## 开发注意事项

1. **工作流修改**：修改 `.github/workflows/auto-release.yml` 后需要推送到 GitHub 才能生效
2. **版本格式**：Release 标签格式为 `v{版本号}`，如 `v2.1.141`
3. **去重机制**：工作流会自动跳过已发布的版本
4. **测试**：可以在 GitHub Actions 页面手动触发工作流进行测试
