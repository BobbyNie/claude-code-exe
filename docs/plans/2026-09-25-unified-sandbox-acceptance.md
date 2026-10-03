# 免安裝統一封裝驗收矩陣

## 2026-10-03 发布形式更新：五产品同一 Release

使用者进一步明确要求：ccode 与 Claude Code、Qwen Code、Codex App、Codex CLI
合并到同一个 Release，且 ccode 使用最新 Claude Code。本更新取代此前“后续独立发布”的要求。
每日流程只检测一次上游版本，以同一个 ClaudeVersion 下载 claude.exe 并构建普通版 ccode，
验收及内嵌引擎 SHA256 一致性检查通过后一起发布。缺少 ccode 或校验材料的旧 bundle 不算完整。
历史独立 Release 保留，不删除既有 Release/tag 来补齐工具包。不新增企业签名门槛；
HTTPS 代理已知限制及 hosted 验收边界保持如实记录。实施不等于实际最新引擎验收已经通过。


## 2026-10-03 普通版发布范围更新

使用者明确要求「按照普通版release就好了」。本次独立发布采用普通公开构建，
不要求企业 signer SPKI、批准公钥指纹或企业签名部署证据；不修改企业版保护逻辑。
完整企业验收与正式信任仍未完成，不得把普通版发布等同全部 A01–A20 通过。
普通版候选、校验、已知代理缺口与使用限制见 [发布范围](2026-10-03-public-release.md)。
此前的 hosted 验收授权及仅延期 HTTPS 代理至 HTTPS API 的记录保持不变。


## 2026-10-03 使用者授權：延期 HTTPS 代理至 HTTPS API

使用者明確要求：「先不考虑支持 `https://` 代理访问 HTTPS API」。
本次獨立 Release 暫不以此組合（代理外層 TLS 加目標內層 TLS）作為放行門檻，
狀態為 **延期／本次不承諾支援**，不是已驗收通過。此前完整 proxy parity 的要求，
僅此組合由本更新取代。已推送的實作候選不構成支援或驗收證據。
直連 HTTPS、`http://` 代理至 HTTPS API、各自的憑證校驗及其他既有放行要求維持不變；
本項授權不推論其他代理組合通過或一併延期。
不得因此停用憑證校驗、靜默繞過明確代理設定直連，或將舊失敗流程改記為成功。
後續 CI 應將此延期組合與本次必要驗收分離，保留測試以供恢復開發時使用。


## 2026-10-03 使用者授權的放行平台更新

使用者已明確要求：「解决统一沙箱版本问题,并发布这个版本为release，GitHub 托管 Windows 环境验收通过即可。」
因此，本次 x64 Release 接受 **GitHub 托管 Windows runner** 的驗收證據；
不再以另行取得 Windows 11 實機、Client SKU 或非 Administrators 帳戶證據作為發布前置條件。
必須如實記錄 runner 的 OS／build、架構、帳戶權限及候選 SHA，不把 Windows Server 結果寫成 Windows 11 普通帳戶結果。
本文及關聯文件中較早的「hosted 只算補充」「必須 Windows 11 普通帳戶實機」要求，
就**本次放行平台與帳戶證據**而言由此更新取代；既有實機腳本可保留為選用驗證。

這項授權不把舊的失敗流程變成通過，不取消 DNS／TLS、資料保全、原始負載、簽章及更新回滾要求，
也不把在線 runner 當作已斷網。原先獨立離線測試仍須在 hosted 環境設計隔離子步驟取得實際證據，
不能僅移除 OS 檢查便標為通過。正式信任及再分發批准若尚未取得，仍須如實列為未完成。
正式 Release 必須對應本分支候選的已驗證提交與附件雜湊；main 的日常混合工具包不是本方案交付證據。

狀態：實施及驗收進行中，尚未放行。逐項狀態見 [證據台帳](2026-09-25-unified-sandbox-evidence.md)。對應 [設計](2026-09-25-unified-sandbox-design.md)。以下是必要測試計畫，不是已通過報告。

最新範圍：程式交付受名稱限制，使用者資料豁免；普通帳戶運行，不安裝驅動或服務。主方案是本機相容性封裝，不宣稱安全沙箱。

2026-09-26 放行平台收斂為 **Windows 11 x64**。所有「Windows 通過」結論必須記錄實際 Windows 11 版本／build 與 x64 架構；`windows-latest` 的 Windows Server 結果只算補充回歸，不算本條平台驗收。其他作業系統與架構不在本次放行範圍。

| 項目 | 測試方法 | 必須結果 |
|---|---|---|
| 離線安裝 | 乾淨目標 Windows 11 x64 普通帳戶解包；停用或物理斷開全部非 loopback 網卡；以獨立 `accept-offline-windows11-x64.ps1` 比對 route／adapter、服務／驅動及 program／data manifests | 不要求提權；零非 loopback default route／Up adapter；不新增服務／驅動；program 只新增已核准且 hash 相符的版本化 runtime；動態資料只進外置 data root；只依賴包內或核准系統組件 |
| 交付名稱 | 從全新路徑以 `build_enterprise_package.py` 組裝 Windows 11 x64 獨立候選；提供 `ccode.exe`、已驗證來源 JSON、中性 usage、逐份必要通知及核准 hash、核准受限名稱；再配對掃描 ZIP／解包鏡像 | 只含白名單檔案；不含混合 bundle、data/profile/runtime/session/temp 或更新殘留；通知原 bytes/hash 不變；名稱或通知衝突 fail closed；audit 為 matched/passed，但不冒稱再分發或簽署已批准 |
| 原始負載 | 執行 `--package-manifest`，比對 embedded resource、實際解出檔案、官方 manifest／payload URL、SHA256、size、engine 與 adapter revision；再執行篡改拒絕案例 | 公開 JSON 與內嵌清單一致，原始內容未被字串／二進位替換破壞；證據檔記錄 extracted hash，且命令不建立 profile/runtime 副作用 |
| 內部範圍 | 在建立 data/profile/runtime 前執行 `--boundary-manifest`；核對公開 exact／prefix、alias expansion、固定子進程值、PE resource 101/102、opaque scan／publisher 狀態與通知來源；把同一 JSON 交給組裝器 | JSON 不含環境值或憑證；明示原始 runtime 名稱仍存在、process tree 並非 name-free；企業 manifest 保存完全一致的 validated boundary；與核准名稱／必要通知衝突時 fail closed，不把公開名稱通過當成內部全部通過 |
| 資料分離 | 對完整企業候選執行 `accept-enterprise-lifecycle-windows11-x64.ps1`：重算 ZIP／解包 audit，啟動會話及六工具，搬移完整程式目錄，再從已運行目錄重新組裝候選 | 原 package 檔案不變；只允許 hash 相符的版本化 runtime；資料／會話只進外置 data root；fresh repack 的解包 path/size/SHA256 與原候選完全一致，不攜入 runtime/data/profile/session/temp |
| 路徑一致 | mkdir、stat、讀寫、列舉、重命名、刪除及子進程讀取同檔 | 同一名稱空間；無 EEXIST／不存在矛盾 |
| 執行檔 | 有空格和非 ASCII 路徑下啟動、引擎自啟動、內建搜尋 | `.exe` 可執行；Grep／Glob 真正返回預期內容 |
| 路徑邊界 | Windows 11 x64 普通帳戶：中文／空格、大小寫、junction；由短 cwd 以 `--workspace` 選取 259+ 本機路徑、UNC、device namespace | 支援的本機路徑保持 UUID／歷史一致；259+、UNC、device 分別只回 `E_WORKSPACE_PATH_TOO_LONG`、`E_WORKSPACE_UNSUPPORTED`、`E_WORKSPACE_PATH`，exit 64、stdout 空、零資料／API 副作用 |
| 基本工具 | 實際 Bash、Read、Write、Edit、Grep、Glob | 結果、退出碼、取消均正確，不只 mock 啟動成功 |
| 串流 JSON | 在每種分片點注入 JSON／UTF-8；最終無效 JSON | 完整後才驗證；無效時清楚報錯、零猜測性工具執行 |
| 工具名稱 | 空名稱、未知名稱、重複 ID、缺終止事件 | 分類錯誤；不回退成任意 shell 或重放副作用 |
| 權限 | 批准、拒絕、等待中取消、前端異常退出 | 不因介面問題繞過批准；可恢復明確狀態 |
| 歷史 | 前端列出、選擇並續接已知會話 | 實際模型請求包含歷史標記；列表可見不算恢復成功 |
| 升級／搬移 | 以相同外置 data/workspace 搬移完整 program directory，搬移後重新取 workspace ID、列出會話並經 loopback fixture 真實 `--continue`；另作兩版本 runtime 更新／回滾 | 程式搬移前後工作區身份一致、原歷史實際送往引擎且可續接；舊資料保留；版本不相容停止；程式搬移案例不可替代跨版本更新／回滾矩陣 |
| 遷移 | 舊 profile、新舊衝突、損壞末行、磁碟滿、中斷 | 原資料雜湊不變；無盲目合併；可從備份重試 |
| 並發 | 同一 session 雙開、多會話同工作區 | 同一 session 單寫入；不互相覆蓋歷史 |
| 擴展 | 技能、子代理、每種核准 MCP 的真實工作流 | 標記具體支援組合，不宣稱所有第三方外掛相容 |
| 網路 | gateway 不可達、TLS 失敗、憑證過期、429、串流斷線 | 分類顯示；寫操作不盲重試；本機封裝不冒稱阻斷所有外連 |
| 診斷 | 所有上述失敗輸出與診斷包 | 無憑證洩漏；產品錯誤不透傳不必要內部品牌路徑 |
| 更新 | 負載篡改、簽名失敗、更新中斷、回退 | 拒绝啟動未通過驗證的候選，保留相容資料快照 |

每個案例記錄 Windows 版本、包版本、引擎版本、adapter 版本、帳戶權限、測試輸入、期望與實際結果及證據位置。測試資料不得用真實憑證。

企業包必須先在連線建置階段取得 `build.ps1` 成功輸出的 `package-provenance.json` 與 `runtime-boundary.json`（尚未簽署 manifest 的企業版不可啟動查詢），並使用法律／合規方提供的實際必要通知及核准 SHA-256；下列 `<核准值>` 不可用測試佔位值代替正式證據。每個禁用名稱需重複提供 `--restricted-name`：

```powershell
# 使用 build.ps1 成功產出的 sidecar；不得繞過未簽署企業版啟動閘門。
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

輸出路徑必須事前不存在。簽署安裝完成後，兩個 manifest 命令仍須證明沒有建立 data/profile/runtime，並與建置 sidecar 完全一致；`runtime-boundary.json` 必須與包內 `manifest.json.runtimeBoundary` 完全一致。正式 Windows 11 x64 驗收需保存 archive SHA-256、`package-audit.json`、解包鏡像、實際核准政策／通知來源及端點 metadata；本機 deterministic 測試或未核准通知不能把 A02／A04 標記通過。

離線案例不得放進仍需 GitHub 網路連線的 workflow 後宣稱自動通過。先在連線狀態準備 repo、Python 3、PowerShell 7 及候選 `ccode.exe`，之後斷開所有非 loopback 網路，再於普通帳戶執行：

```powershell
pwsh ./scripts/ccode/accept-offline-windows11-x64.ps1 `
  -Executable ./ccode.exe `
  -ExpectedEngineVersion 2.1.282 `
  -AdapterRevision <完整提交 SHA> `
  -EvidencePath D:/ccode-evidence/offline-windows11-x64.json
```

輸出的 loopback fixture 證據只證明本地 deterministic model response 下的真實 engine／tool 行為，不替代正式企業 gateway、真實模型、TLS 或出口政策驗收。腳本存在或本機 static test 綠燈也不等於 Windows 11 x64 實機已通過。

完整企業候選的資料分離／程式搬移／重新打包另在同一類 Windows 11 x64 普通帳戶端點執行：

```powershell
pwsh ./scripts/ccode/accept-enterprise-lifecycle-windows11-x64.ps1 `
  -CandidateRoot D:/ccode-candidate `
  -SignaturePath D:/ccode-signatures/manifest.sig `
  -PublicKeyPath D:/ccode-policy/approved-signer.der `
  -TrustedPin <獨立核准的-SPKI-DER-SHA256> `
  -NodeCommand D:/approved-tools/node.exe `
  -EvidencePath D:/ccode-evidence/enterprise-lifecycle-windows11-x64.json
```

`CandidateRoot` 必須是 `build_enterprise_package.py` 的完整輸出根，包含 `unpacked/`、`package-audit.json` 及唯一 ZIP。驗收器以 `enterprise_lifecycle.py inspect` 不信任地重算 archive／unpacked audit 與 manifest hashes，複製完整 `unpacked/` 到中文／空格 program path，使用外置 data/workspace 執行真實 engine／六工具，搬移整個 program directory 後核對相同 workspace ID、列出並真實續接既有歷史；最後從已運行的程式目錄重新組裝 fresh candidate，並以 `compare` 強制解包 path/size/SHA256 及 manifest 完全一致。ZIP byte digest 會分別記錄；跨 Python／zlib 工具鏈時不以壓縮 bytes 相同作唯一通過條件。此案例仍使用 deterministic loopback fixture，且未在實際 Windows 11 x64 成功執行前不得把 A05 標為通過。

既有兩版本 Windows 回歸證據只能沿用到未改變的舊功能，不替代新前端、新啟動環境及名稱掃描。`windows-latest` 可驗證補充回歸，但路徑邊界仍須在真實 Windows 11 x64 普通帳戶 runner 以 `--workspace-boundary-only` 取得成功結果；歷史 `WinError 267` 不是通過證據。A03 來源清單須由 Windows 11 x64 workflow 產出 `package-provenance.json`；只有程式碼或 Windows Server 結果不能標記通過。任何代碼修改遵守先失敗測試、再最小實作、再回歸的 TDD 流程。


A19 診斷驗收須對每個故障案例使用事前不存在的路徑，例如：

```powershell
./ccode.exe --diagnostics D:/ccode-evidence/failure.json --print "synthetic prompt"
```

每份 JSON 必須能以 UUID 解析 `operationId`，只含中性 `errorCode`／`category`／`exitCode` 及明確隱私旗標；不得含測試 token、prompt、使用者根、workspace/data 絕對路徑或 raw exception。以既有 sentinel 檔重跑時，sentinel bytes 必須不變、stderr 額外包含 `E_DIAGNOSTIC_WRITE`，主要 exit code 不變。2026-10-01 已完成產生器、純函式測試與 Windows 驗收腳本案例；尚未取得 Windows 11 x64 普通帳戶的實跑 JSON，亦未覆蓋 TLS、DNS、過期憑證、429、串流、資料損壞、政策拒絕全部案例，故 A19 不得標為通過。

企業生命週期驗收入口現在強制使用 `inspect-signed`，要求外部 detached
signature、canonical Ed25519 SPKI DER 及獨立核准的公鑰 SHA256 pin。
簽署 bytes 為 UTF-8 `ccode-enterprise-manifest-v1`、單一 NUL byte、原始
manifest bytes 的串接。不得從候選公鑰自行生成 pin 並冒稱核准信任。
未通過簽章或候選 audit 不建立驗收工作目錄、不複製／執行候選；通過後
仍在第一次執行前核對 copied files 的 path/size/SHA256。工程 Node
runtime 必須事先備妥並由端點政策核准；CI 固定 22.23.3 不代表端點已
具備或核准它。外部驗收器不能替代 ccode.exe 內建啟動／更新 gate。
2026-10-02 核對確認企業 build 已接入原生啟動 gate：編譯期 signer policy、
所有入口前簽章／inventory／PE resource 驗證，以及 invocation 期間持有
靜態檔案句柄。Run36988203229/c8aa21b 兩版 hosted Windows 的公開 RFC8032
fixture 測試通過合法簽署 metadata 入口、外置 data、缺失／無效簽章、
通知及 manifest bytes 篡改拒絕。這些案例不是完整更新流程，也不代表
正式信任批准；A20 尚未完成。正式 signer SPKI／獨立 pin、實際通知／
再分發批准、完整更新中斷／回退矩陣及 Windows11 普通帳戶實跑仍須取得。
任意並行替換防護也不能只由上述入口測試推論。
