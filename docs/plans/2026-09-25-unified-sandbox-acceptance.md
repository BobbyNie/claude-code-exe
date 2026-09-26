# 免安裝統一封裝驗收矩陣

狀態：實施及驗收進行中，尚未放行。逐項狀態見 [證據台帳](2026-09-25-unified-sandbox-evidence.md)。對應 [設計](2026-09-25-unified-sandbox-design.md)。以下是必要測試計畫，不是已通過報告。

最新範圍：程式交付受名稱限制，使用者資料豁免；普通帳戶運行，不安裝驅動或服務。主方案是本機相容性封裝，不宣稱安全沙箱。

2026-09-26 放行平台收斂為 **Windows 11 x64**。所有「Windows 通過」結論必須記錄實際 Windows 11 版本／build 與 x64 架構；`windows-latest` 的 Windows Server 結果只算補充回歸，不算本條平台驗收。其他作業系統與架構不在本次放行範圍。

| 項目 | 測試方法 | 必須結果 |
|---|---|---|
| 離線安裝 | 乾淨目標 Windows 11 x64 普通帳戶解包；停用或物理斷開全部非 loopback 網卡；以獨立 `accept-offline-windows11-x64.ps1` 比對 route／adapter、服務／驅動及 program／data manifests | 不要求提權；零非 loopback default route／Up adapter；不新增服務／驅動；program 只新增已核准且 hash 相符的版本化 runtime；動態資料只進外置 data root；只依賴包內或核准系統組件 |
| 交付名稱 | 從全新路徑以 `build_enterprise_package.py` 組裝 Windows 11 x64 獨立候選；提供 `ccode.exe`、已驗證來源 JSON、中性 usage、逐份必要通知及核准 hash、核准受限名稱；再配對掃描 ZIP／解包鏡像 | 只含白名單檔案；不含混合 bundle、data/profile/runtime/session/temp 或更新殘留；通知原 bytes/hash 不變；名稱或通知衝突 fail closed；audit 為 matched/passed，但不冒稱再分發或簽署已批准 |
| 原始負載 | 執行 `--package-manifest`，比對 embedded resource、實際解出檔案、官方 manifest／payload URL、SHA256、size、engine 與 adapter revision；再執行篡改拒絕案例 | 公開 JSON 與內嵌清單一致，原始內容未被字串／二進位替換破壞；證據檔記錄 extracted hash，且命令不建立 profile/runtime 副作用 |
| 內部範圍 | 在建立 data/profile/runtime 前執行 `--boundary-manifest`；核對公開 exact／prefix、alias expansion、固定子進程值、PE resource 101/102、opaque scan／publisher 狀態與通知來源；把同一 JSON 交給組裝器 | JSON 不含環境值或憑證；明示原始 runtime 名稱仍存在、process tree 並非 name-free；企業 manifest 保存完全一致的 validated boundary；與核准名稱／必要通知衝突時 fail closed，不把公開名稱通過當成內部全部通過 |
| 資料分離 | 啟動、會話、工具及更新後比較程式目錄 | 動態資料進資料區；重新交付不攜帶個人資料 |
| 路徑一致 | mkdir、stat、讀寫、列舉、重命名、刪除及子進程讀取同檔 | 同一名稱空間；無 EEXIST／不存在矛盾 |
| 執行檔 | 有空格和非 ASCII 路徑下啟動、引擎自啟動、內建搜尋 | `.exe` 可執行；Grep／Glob 真正返回預期內容 |
| 路徑邊界 | Windows 11 x64 普通帳戶：中文／空格、大小寫、junction；由短 cwd 以 `--workspace` 選取 259+ 本機路徑、UNC、device namespace | 支援的本機路徑保持 UUID／歷史一致；259+、UNC、device 分別只回 `E_WORKSPACE_PATH_TOO_LONG`、`E_WORKSPACE_UNSUPPORTED`、`E_WORKSPACE_PATH`，exit 64、stdout 空、零資料／API 副作用 |
| 基本工具 | 實際 Bash、Read、Write、Edit、Grep、Glob | 結果、退出碼、取消均正確，不只 mock 啟動成功 |
| 串流 JSON | 在每種分片點注入 JSON／UTF-8；最終無效 JSON | 完整後才驗證；無效時清楚報錯、零猜測性工具執行 |
| 工具名稱 | 空名稱、未知名稱、重複 ID、缺終止事件 | 分類錯誤；不回退成任意 shell 或重放副作用 |
| 權限 | 批准、拒絕、等待中取消、前端異常退出 | 不因介面問題繞過批准；可恢復明確狀態 |
| 歷史 | 前端列出、選擇並續接已知會話 | 實際模型請求包含歷史標記；列表可見不算恢復成功 |
| 升級／搬移 | 重啟、程式目錄搬移、runtime 更新／回滾 | 工作區身份穩定；舊資料保留；版本不相容停止 |
| 遷移 | 舊 profile、新舊衝突、損壞末行、磁碟滿、中斷 | 原資料雜湊不變；無盲目合併；可從備份重試 |
| 並發 | 同一 session 雙開、多會話同工作區 | 同一 session 單寫入；不互相覆蓋歷史 |
| 擴展 | 技能、子代理、每種核准 MCP 的真實工作流 | 標記具體支援組合，不宣稱所有第三方外掛相容 |
| 網路 | gateway 不可達、TLS 失敗、憑證過期、429、串流斷線 | 分類顯示；寫操作不盲重試；本機封裝不冒稱阻斷所有外連 |
| 診斷 | 所有上述失敗輸出與診斷包 | 無憑證洩漏；產品錯誤不透傳不必要內部品牌路徑 |
| 更新 | 負載篡改、簽名失敗、更新中斷、回退 | 拒绝啟動未通過驗證的候選，保留相容資料快照 |

每個案例記錄 Windows 版本、包版本、引擎版本、adapter 版本、帳戶權限、測試輸入、期望與實際結果及證據位置。測試資料不得用真實憑證。

企業包必須先在連線建置階段取得 `ccode.exe --package-manifest` 與 `ccode.exe --boundary-manifest` 的成功 JSON，並使用法律／合規方提供的實際必要通知及核准 SHA-256；下列 `<核准值>` 不可用測試佔位值代替正式證據。每個禁用名稱需重複提供 `--restricted-name`：

```powershell
./ccode.exe --package-manifest > ./package-provenance.json
if ($LASTEXITCODE -ne 0) { throw 'package provenance failed' }
./ccode.exe --boundary-manifest > ./runtime-boundary.json
if ($LASTEXITCODE -ne 0) { throw 'runtime boundary failed' }
python ./scripts/ccode/build_enterprise_package.py `
  --executable ./ccode.exe `
  --provenance ./package-provenance.json `
  --boundary ./runtime-boundary.json `
  --usage ./docs/ccode-enterprise-usage.md `
  --notice <必要通知檔> `
  --notice-sha256 <核准通知SHA256> `
  --restricted-name <核准禁用名稱> `
  --output ./enterprise-candidate
if ($LASTEXITCODE -ne 0) { throw 'enterprise package assembly failed' }
```

輸出路徑必須事前不存在。兩個 manifest 命令都必須證明沒有建立 data/profile/runtime；`runtime-boundary.json` 必須與包內 `manifest.json.runtimeBoundary` 完全一致。正式 Windows 11 x64 驗收需保存 archive SHA-256、`package-audit.json`、解包鏡像、實際核准政策／通知來源及端點 metadata；本機 deterministic 測試或未核准通知不能把 A02／A04 標記通過。

離線案例不得放進仍需 GitHub 網路連線的 workflow 後宣稱自動通過。先在連線狀態準備 repo、Python 3、PowerShell 7 及候選 `ccode.exe`，之後斷開所有非 loopback 網路，再於普通帳戶執行：

```powershell
pwsh ./scripts/ccode/accept-offline-windows11-x64.ps1 `
  -Executable ./ccode.exe `
  -ExpectedEngineVersion 2.1.282 `
  -AdapterRevision <完整提交 SHA> `
  -EvidencePath D:/ccode-evidence/offline-windows11-x64.json
```

輸出的 loopback fixture 證據只證明本地 deterministic model response 下的真實 engine／tool 行為，不替代正式企業 gateway、真實模型、TLS 或出口政策驗收。腳本存在或本機 static test 綠燈也不等於 Windows 11 x64 實機已通過。

既有兩版本 Windows 回歸證據只能沿用到未改變的舊功能，不替代新前端、新啟動環境及名稱掃描。`windows-latest` 可驗證補充回歸，但路徑邊界仍須在真實 Windows 11 x64 普通帳戶 runner 以 `--workspace-boundary-only` 取得成功結果；歷史 `WinError 267` 不是通過證據。A03 來源清單須由 Windows 11 x64 workflow 產出 `package-provenance.json`；只有程式碼或 Windows Server 結果不能標記通過。任何代碼修改遵守先失敗測試、再最小實作、再回歸的 TDD 流程。
