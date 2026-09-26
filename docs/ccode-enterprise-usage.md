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
