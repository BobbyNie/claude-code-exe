# ccode for Windows 11 x64

This delivery is a portable command-line package for 64-bit Windows 11 (build 22000 or newer).

## Start

Run `ccode.exe` from a normal, non-administrator account. The current directory is the default workspace. Use `--workspace PATH` to select another supported local workspace and `--data-dir PATH` to select an external data root.

## Data boundary

The delivery directory is program-only. Profiles, runtime material, sessions, logs, temporary files, and other changing state belong in the selected external data root. Do not add endpoint data to a redistributed archive.

## Package identity

Run `ccode.exe --package-manifest` to print the embedded engine and adapter provenance. The delivery `manifest.json` records package file hashes and the Windows 11 x64 target. Hash records do not replace publisher signatures, redistribution approval, or required legal notices.

## Diagnostics

Keep credentials and private prompts out of shared diagnostics. Record the package hash, engine version, adapter revision, Windows build, account privilege state, operation, neutral error code, and trace identifier when reporting a failure.

## 建置與簽署前的封裝輸入

`build.ps1` 成功後在 `ccode.exe` 同目錄輸出 `package-provenance.json`
與 `runtime-boundary.json`。前者直接複製嵌入 resource 102 的原始文件；後者
由獨立建置工具從 launcher 共用的 boundary 定義產生。將這兩個 sidecar
分別作為 `build_enterprise_package.py --provenance` 與 `--boundary` 的輸入。

尚未具有有效簽署 manifest 的企業版不得用來執行 metadata／boundary 查詢。
Sidecar 不會繞過企業版啟動閘門，也不構成簽章、正式 signer 批准或驗收證據；
封裝仍須完成 manifest 簽署及原有安裝驗證。
