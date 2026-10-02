# A 方案實施與驗收證據台帳

日期：2026-09-25。**狀態：實施中，未放行。**
2026-09-26 範圍更新：最終放行平台限 **Windows 11 x64**；其他平台不是門檻。現有 GitHub `windows-latest`／Windows Server 證據不得標作 Windows 11 實機通過。

本台帳追蹤 `2026-09-25-unified-sandbox-design.md` 的 A 方案及完整
`2026-09-25-unified-sandbox-acceptance.md`。不得以單一綠色 CI 代替以下所有門檻。
B/C、遠端檔案同步不屬選定 A 方案；不能用這個排除理由省略 A 的工作區身份、
原生檔案語義、資料安全、子進程管理及名稱要求。

## 證據使用規則

- 「部分」不是通過；缺資料、未跑、只有程式碼／測試存在，均不得標為完成。
- Windows CI 固定測試引擎 `2.1.221`、`2.1.282`；每次以實際 run 的 SHA 為準。
- 本機 C++／Python 測試不替代 Windows 執行結果。
- 本機 API fixture 僅取代模型回覆：實際引擎、工具、子進程、讀寫和 profile 可以是真實的，
  但不能宣稱因此完成真實模型／第三方 gateway／企業端點驗收。
- 所有 API fixture 使用假 token。不能把提示、原始工具輸出或真實憑證放进公開 CI 證據。
- 簽署者信任、再分發權限、目標端點和試運行必須有外部可核對證據，不能自行假設批准。

## 全量驗收追蹤

| ID | 原矩陣項目 | 現有實作／證據入口 | 尚欠證據或實作（全部保留為門檻） |
|---|---|---|---|
| A01 | 離線安裝 | 資源內嵌引擎；Windows 建置／啟動；已實作獨立斷網驗收器，檢查 Windows 11 x64 普通帳戶、default route／adapter、服務／驅動差異及真實 engine／tool 試運行 | 尚須乾淨 Windows 11 x64 普通帳戶真正斷網實跑並保存成功 evidence；loopback fixture 不替代企業 gateway／真實模型 |
| A02 | 交付名稱 | 中性入口及 help；`package_audit.py` 已實作 ZIP／解包唯讀配對掃描；`build_enterprise_package.py` 已實作 Windows 11 x64 白名單組裝、必要通知 hash 綁定、runtime boundary 驗證及固定 metadata ZIP；舊混合公共發布 workflow 已移除；本機 13 項 audit＋6 項組裝測試通過 | 核准名稱政策、核准必要通知／再分發權、正式獨立發布流程、實際 Windows 11 x64 ZIP／解包掃描及更新殘留核實；工具測試不等於放行 |
| A03 | 原始負載 | build 上游 checksum；啟動資源 SHA256、解出內容逐位元組校驗；已實作 schema 1 隨包來源清單、`--package-manifest` 及 extracted hash 證據 fixture | 尚須 Windows 11 x64 普通帳戶兩版本實跑並保存 `package-provenance.json`；來源清單未簽名，不替代 A20 可信簽署 |
| A04 | 內部範圍 | ADR 明確允許引擎／使用者資料原名；已實作 side-effect-free `--boundary-manifest`，固定記錄公開 exact/prefix、子進程 alias／fixed values、PE resource 101/102、opaque scan／簽署狀態及通知來源；企業組裝器必須驗證並保存同一 boundary | 尚須 Windows 11 x64 實機證明命令零 data/profile/runtime 副作用，並以核准受限名稱／必要通知完成衝突核實；原始 runtime 名稱存在及未掃描 opaque binary 明示為限制，A04 尚未通過 |
| A05 | 資料分離 | --data-dir、獨立 profile；工具 integration 檢查程式區無 session；離線驗收器記錄 program before／after、外置 data 及 workspace manifest，僅允許 hash 相符的版本化 runtime | 尚須 Windows 11 x64 離線實跑；更新、搬移、重新打包排除資料及程式區無 temp 的完整矩陣仍欠 |
| A06 | 路徑一致 | 原生 mkdir/stat/讀寫/列舉/刪除/子進程；`05750fe` 原生 rename；`7d579ce` / `36184360358` 兩版本一般／case／junction 的真實 Bash mv→Read→Edit→Grep→Glob 及失敗 rename 保全通過 | 本機 API fixture 不替代真實模型／企業 gateway；完整故障矩陣仍欠，不代表長 cwd 通過 |
| A07 | 執行檔 | `36123600008` 兩版本通過：空格／中文程式與工作區、前端→真正 Grep/Glob；既有自啟動測試亦通過 | 本條所列 CI 場景通過；乾淨端點證據仍依 A01 |
| A08 | 路徑邊界 | 工具 fixture 含中文／空格；`87f7065` / `36135890793` 兩版本快照／候選超過 260 字元；`92f3e3a` / `36175453668` 大小寫及 junction 下六工具、UUID、列表與續接通過；已實作 `--workspace` 與 258／259、UNC、device namespace 明確邊界 | 新邊界只有本機 C++／workflow fixture 證據，仍須 Windows 11 x64 普通帳戶兩版本實跑；歷史長 cwd `WinError 267` 不算產品通過，亦不把檔案長路徑等同 process cwd 支援 |
| A09 | 基本工具 | tools-integration.py 驅動真實 Write/Edit/Read/Grep/Glob/Bash；一般完整路徑成功、8.3 短路徑無批准拒絕；特定 console 批准／拒絕／取消見 A12 | 全工具政策／取消矩陣、實際 gateway 試運行仍欠；A12 間歇停滯尚未解決 |
| A10 | 串流 JSON | UTF-8／工具 JSON 每個 byte 分片點、失敗不可復活；`3be31ae` / `36163996525` 每行 16 MiB 邊界及合併／分片等價通過；`36173166133`、`36174265807` 真實引擎截斷及正常 EOF 缺終止案例通過 | 特定 fixture 不代表所有串流／擴展狀態或企業 gateway；完整故障矩陣仍欠 |
| A11 | 工具名稱 | 空／未知工具名、空／重複 ID、錯誤參數分類；`be9f26a` / `36186966481` 重複宣告拒絕；`7599787` / `36188226673` 重複及 result 後 init 拒絕；缺 block 終止案例見 A10 | 所有狀態轉移、新版本／擴展的真實事件相容仍欠；前端拒絕不等於能撤銷引擎已執行副作用 |
| A12 | 權限 | 預設拒絕、interactive console、Job Object；既有兩版本批准／拒絕／取消及真實後代回收通過；`8a2ac80` / `36185827200` 數值白名單進程診斷 Windows API 通過 | `36184360358` 新版 console 空白停滯、kill wait 逾時及鎖占用未定位；後續綠燈不代表修復。`b0ad018` / `36192125436` 兩版本 RPC 及診斷測試回歸通過，但前輪 unavailable 原因仍未確認；目標普通帳戶端點仍欠 |
| A13 | 歷史 | --sessions／--resume／--continue／picker、工作區 UUID；`bcd18a5` 已知歸屬損壞會話 unavailable 及拒絕；`ad17981` / `36188662312` 固定 resume 身份、健康兩輪歷史及跨版本續接回歸通過 | 未知歸屬損壞仍整次拒絕；完整支援版本、損壞恢復及企業端點證據仍欠 |
| A14 | 升級／搬移 | `ee9e8b5` / `36159427257` 真正 2.1.221 → 2.1.282 → 2.1.221 回退、程式目錄及外置資料根搬移後續接通過；不相容引擎拒絕 | 工作區本身搬移、完整支援版本／隨包相容 manifest、企業端點實測 |
| A15 | 遷移 | SHA-256 快照、隔離候選、全會話 preflight／恢復、來源變動拒絕、原子切換及回退；`c17b873` / `36190191804` 兩版本公開 --archive-workspace-pending 保全原 bytes／已提交身份及重試通過 | 重名衝突完整分類、磁碟滿／強制中斷邊界與重試、企業真實資料／gateway 驗收；預置 pending 或檔案替換失敗不等於斷電 |
| A16 | 並發 | 已實作 profile shared／維護 exclusive、metadata 短鎖及每 session 單 writer；`concurrency-integration.py` 使用真實引擎與 barrier fixture 驗證新 session 固定 UUID、同 session 拒絕及不同 session 並行 | 本機 helper／回歸已通過；仍欠 Windows 11 x64 上 2.1.221、2.1.282 的實際執行證據，不得以 Windows Server 或程式存在代替 |
| A17 | 擴展 | CLI 參數可接設定／MCP／agents | 技能、子代理、核准 MCP 真實流程及明確版本相容矩陣 |
| A18 | 網路 | gateway 設定入口；實際引擎本機 fixture 已覆蓋不可達、401、429、傳輸截斷、正常 EOF 未完成／完整參數但缺終止；指定案例要求非零退出、無寫入／洩漏，HTTP 案例一次模型請求 | TLS／DNS／過期憑證及完整分類、企業核准端點／部署政策仍欠；不得推廣為所有故障無重放，亦不宣稱 OS 網路隔離 |
| A19 | 診斷 | 中性錯誤碼及 parser 內容不直接外洩；已實作 `--diagnostics PATH` create-new JSON、UUID operation ID、分類、exit code、四項 privacy=false、敏感內容不落檔及 `E_DIAGNOSTIC_WRITE` 不遮蔽主要退出碼；本機純函式／contract 已通過 | 尚須 Windows 11 x64 普通帳戶實跑並保存 JSON，且逐一覆蓋 A01-A18 的 TLS／DNS／429／串流／資料損壞／政策拒絕等全部失敗；目前單一 gateway 缺設定案例及程式存在不能標記通過 |
| A20 | 更新 | SHA256、版本化 runtime | 可信簽名 manifest、固定依賴、簽名失敗、中斷、相容資料快照回退 |

## 設計額外要求與放行條件

| ID | 要求 | 目前缺口／驗證方式 |
|---|---|---|
| D01 / R01 | 免安裝、無需管理員 | A01；CI 管理員執行不等於普通帳戶通過 |
| D02 / R02–03 | 中性企業交付及完整公開邊界 | A02/A04；不可沿用混合品牌公共 bundle |
| D03 / R04 | 原工具及擴展穩定 | A09/A17；明列支援和未支援命令，不宣稱 TUI 完全等價 |
| D04 / R05 | 穩定工作區 UUID、會話權威檔案 | 持久 UUID 與可重建會話索引已整合，`ae5ba72` / `36132604878` 兩版本 Windows 通過；仍欠搬移遷移與完整生命週期；A13–16 |
| D05 / R06 | 核准 API 通道 | A18；普通本機封裝本身無法封鎖工具任意外連，部署政策另驗 |
| D06 / R07 | 受控完整進程樹 | A12；Job Object 結束／取消／崩潰無孤兒進程 |
| D07 / R08 | 更新、診斷、恢復 | A14/A15/A19/A20 |
| D08 | 固定 adapter／引擎／工具鏈及 mapping manifest | CI 固定兩引擎；已實作 adapter commit、engine hash／size、官方 manifest／payload 來源的隨包 manifest，尚待 Windows 11 x64 實跑及完整工具鏈／mapping 清單 |
| D09 | 工具 schema、錯誤分类、不猜測／不重放 | A10/A11/A18；前端驗證輸出不等於能阻止引擎內部已执行的工具 |
| D10 | 隔離候選 profile、備份 hash、原子 active 切換 | 已實作全會話驗證、來源變動拒絕、原子切換及保全新版資料的明確回退；`36159427257` 有實際兩引擎證據。磁碟滿／強制中斷、真實企業資料及完整故障範圍仍未完成 |
| D11 | 來源、授權、必要通知、可信簽名 | 需核實再分發權限及核准簽署者；不得刪除必要通知以通過名稱掃描 |
| D12 | 小範圍試運行與中斷／還原／回退演練 | 尚未執行；需明確目標 Windows 版本／帳戶／工作負載與性能基線 |
| D13 | 每案可追溯報告 | Windows、package、engine、adapter、帳戶、輸入、期望、實際、證據位置均須記錄 |

## 已知驗證快照

- `b1a83d9`：Windows CI `36121879495` 兩引擎通過既有建置、runtime/resume 和 portable contract。
  **不包含 A01–A20 的全量驗收**。
- `2f0da2c`：本機六組 C++、八項 Python 通過。新增測試先失敗再實作；分類
  JSON/schema/tool name/input/id/duplicate/unknown，UTF-8 每個 byte 分片點及失敗後禁止復活。
- `27b1945`：新增真正工具端到端 fixture。`36122828477` 兩版本均失敗；失敗不能忽略。
- `5cfaf65` / `36123289630`：相同 workspace/profile/gateway 下對照未改動引擎，
  確認 `RUNNER~1` 8.3 路徑會觸發原生「suspicious Windows path pattern」人工批准政策。
  並非允許規則遺失。不採用 bypass permission 的解法。
- `a195c81` / `36123600008`：兩版本 Windows 全步驟通過。分開保留：
  1. canonical 完整路徑、中文／空格 program/workspace、外置 profile；真正 Write/Edit/Read/Grep/Glob/Bash
     透過本機 API fixture 逐一執行，驗證 tool_result 與實際磁碟內容。
  2. 8.3 短路徑需批准時，無互動批准必須拒絕 Write/Bash，磁碟不能出現寫入副作用。
  工具參數使用分片 SSE JSON。此 run 不證明真實模型／外部 gateway／其他 MCP 已驗收。
- `0097a5a`：本機紅／綠驗證不完整／截斷資料流結束後不可被後續 input 復活；
  包含該修復的 `54a39e8` / `36123883791` Windows 兩版本回歸成功。

- `e22cd72` / `36124014490`：Windows 兩引擎完整 job 均成功（2026-09-25）。
  日誌均明確包含 cancel 與 crash 的 PASS。測試先等待真實 Bash 啟動的
  PowerShell 寫出 PID，以程序 handle 確認存活，再分別發送 Ctrl+Break 或強制終止前端。
  驗證後代 handle 在五秒內 signaled、取消退出碼 130 及 Cancelled 訊息、
  延遲寫入檔案不存在。強制終止不依賴前端正常清理流程。
  這證明本案例的真實後代回收；不證明所有擴展進程、互動批准流程或普通帳戶端點已驗收。

- `c7609a7` / `36124549452`：新增真正 Windows console 批准／拒絕／等待取消案例；
  兩版本均在第一個批准案例失敗：前端輸出 Tool request 後，60 秒內測試 console 未見批准提示。
  因第一案失敗，後續拒絕及等待取消尚未執行，不能標記通過。
- `7a2fe44` / `36128862191`：保留 console buffer 診斷後仍失敗；觀察到 console 為空，
  前端仍存活且停在 Tool request。尚不能判定是 MCP worker 的 console 歸屬還是其他通訊問題。
  `e1aeec8` / `36129256405` 兩版本失敗診斷確認：權限 worker 存活但不在前端 console PID 集合；未更改產品權限政策。
  本機六組 C++、八項 Python 及新增 Python 語法檢查通過，不能替代此失敗的 Windows 互動測試。

- `cacdb06` / `36129711620`：修復 worker 明確 AttachConsole 至前端 PID，保留 MCP 標準輸入／輸出
  pipe；找不到前端 console 時拒絕，不改成自動批准。兩固定引擎的全部 Windows 步驟通過。
  真實 console 案例先確認檔案不存在且畫面已顯示該唯一檔名的批准提示，再注入按鍵：
  `yes` 後實際 Write 內容相符；`no` 後 tool_result 為拒絕且無檔案；等待批准時
  Ctrl+Break 後輸出 Cancelled、返回 130、無檔案。退出後 stdout/stderr 讀取者結束，
  沒有後代持有管道阻止退出。既有工具與進程樹取消／crash 案例仍通過。
  此為 Windows CI 帳戶及固定工具組合的證據，不擴大成企業端點或全部 MCP 的驗收。

- `be8ee3c` / `36130126715`：兩版本 Windows 成功；空／無效／溢位／不可用 frontend PID
  均快速回傳 deny，不退回隱藏 console 等待批准。移除一次性進程列舉診斷。
- `199b57b` / `36130341477`：兩版本 Windows 成功；三種中斷（工具執行中取消、前端 crash、
  等待批准時取消）後，不刪 lock、不修補檔案，使用同一 profile 重啟；歷史可列出、
  新的真實 Write 成功、既有 JSONL 逐位元組不變，沒有重放原取消工具。
  這證明故障後可開新會話，不代表已驗證續接被中斷的那一會話。
- 工作區身份第一個切片：`--workspace-id` 持久 UUID v4，registry 放在獨立 profile，
  規範化工作區路徑作查找鍵，程式位置／runtime hash 不參與身份。原子替換 registry，
  損壞／未知 schema 停止而非重建 ID；本機 TDD 另捕捉並修復暫存 symlink 覆寫風險。
  已補 Windows 程式位置變更及重啟的 CLI 測試，`dac7a06` / `36131094843` 兩版本通過。**工作區本身搬移仍需明確遷移，
  會話仍按引擎 cwd 列舉；本切片不宣稱 A13–A16 或完整穩定身份生命週期已完成。**

- `d1e8855` / `36130850626`：Windows 測試 fixture 使用文字模式還原 registry，造成
  LF→CRLF，逐位元組比較失敗。`dac7a06` 改用 binary 還原，增加還原後立即比對，
  並拆開 symlink 保護斷言，沒有放寬預期。`36131094843` 兩版本全部步驟成功；
  日誌確認工作區 UUID、權限、實際工具和三種中斷後恢復案例均 PASS。
- 歷史損壞分類切片：新增 user metadata 的 type/sessionId/cwd/isSidechain 型別錯誤測試，
  本機先在 classified 斷言失敗，再加入 `E_SESSION_DATA` 明確分類後通過；
  不回傳內容、不改寫原始 JSONL。新增 Windows CLI 測試要求退出碼 64、只有中性錯誤碼
  且原始檔案逐位元組不變。`8579230` / `36131756172` 兩版本 Windows 成功，
  `6c92fdd` / `36131964792` 亦確認該案例 PASS；不代表截斷末行、完整歷史可用性分類、
  遷移或會話索引已完成。

- 歷史選擇／多輪驗收擴充：`resume-integration.py` 從真實 `--sessions` 輸出取得序號，
  經 `--resume` picker 選擇已知歷史，送出兩輪，再重啟 `--continue`。
  每一步檢查實際模型請求的歷史 messages，而不是只看輸出或 session ID；另檢查舊來源
  JSONL 逐位元組不變，測試環境移除繼承的供應商／前端設定後只放假 token。
  這是既有功能的新增驗收測試，沒有先改產品實作；`6c92fdd` / `36131964792`
  兩版本 Windows 全部步驟成功，日誌均確認 picker 多輪和重啟 continue 的 PASS。
  此案例透過 stdin 操作文字 picker，不等同真實 console 按鍵測試；API 為本機 fixture。

- 工作區會話索引整合：`session-index.hpp` 在 profile 鎖內，以持久 workspace UUID
  建立 `session-index/<uuid>.json`，接入啟動列表及 picker。索引只存 ID、摘要、原生
  修改時間、來源引擎版本與 discovered 狀態；discovered 不宣稱版本相容或已恢復。
  每次由原生 JSONL 重建，不讀快取作會話存在／續接判定；刪除／損壞快取不改寫歷史。
  本機 TDD 先確認新入口缺失，再完成實作及六組 C++ 回歸；
  `ae5ba72` / `36132604878` 兩版本 Windows 全步驟成功，日誌確認原生歷史不变及索引重建 PASS。
  UUID 已接入列表／picker 的索引生命週期，仍未完成工作區搬移、相容性判定及候選遷移。

- 停寫快照切片：新增 `--snapshot-profile`，在既有 profile 排他鎖內複製至唯一
  `.pending` 目錄，核對來源／副本 SHA-256、檔案清單及大小後才 rename 發布；不讀取或
  修補會話 JSONL，不切換 active profile。快照含相對路徑 hash manifest，排除 root
  frontend.lock；拒絕 symlink／非普通檔案和來源目錄內的快照目的地。
  本機先新增入口缺失的紅測試，再通過快照、禁止覆寫、注入讀取失敗／複本不符、保留
  未完成候選及新 ID 重試測試。Windows CLI 測試用 Python hashlib 獨立驗證所有副本及
  SHA-256，結果待驗。這不是引擎驗證候選、active pointer、磁碟滿實測或完整遷移；
  `.pending` 失敗目錄保留於資料區，尚無自動清理／斷點恢復政策。

- `9bedd2c` / `36133171241`：兩版本 Windows 在 profile C++ 測試以
  `-1073740791` 結束，尚未執行 CLI 快照驗收，不能標記快照通過。
  檢查 fixture 發現讀取 manifest 的串流在刪除暫存根目錄前未關閉；
  `51a9ae9` 關閉該串流並補頂層例外診斷，本機測試通過，Windows run
  `36133481798` 兩版本全部成功，日誌確認 SHA-256 獨立核對 PASS。
  不修改產品邏輯或放寬原有斷言；紅燈排除後恢復候選實作。

- 隔離候選切片：新增 `--stage-profile <snapshot-id>`。先驗 manifest schema、UUID、
  相對路徑、檔案集合、大小及 SHA-256，再複製到獨立 staging；複本及來源再次核對後
  才發布 `candidates/<id>`，candidate metadata 標記 staged 和來源 snapshot ID。
  不修改 active profile／原快照，也不把 staged 當作引擎已驗證。
  本機 TDD 捕捉缺失介面及 manifest UUID 與目錄身份不一致問題，修復後六組 C++、
  八項 Python 回歸通過。Windows 新增完整副本核對及篡改／缺檔／多檔／穿越拒絕案例，
  結果待驗。仍需實際引擎驗證歷史、可信驗證狀態與原子 active 切換／回滾。

- 候選真實引擎恢復驗收：擴充 resume fixture，在已有多輪歷史後建立快照及隔離候選，
  用候選的獨立 data-dir 續接已知 session。檢查實際模型請求包含舊標記及所有新增輪次，
  新輪次只寫入候選；原 profile 與整份快照前後逐位元組一致。候選仍保持 staged，
  不產生 active pointer。此為本機 API fixture 下的新增驗收，Windows 結果待驗；
  尚非產品內建的可信驗證回執或啟用機制。

## 下一批實施順序

1. 驗證實際工具端到端，處理揭露的缺陷；補取消／權限／進程樹及路徑邊界。
2. 工作區與 profile 生命週期：穩定身份、候選遷移、驗證／原子切換、備份／回滾、並發。
3. 完整診斷與網路故障、擴展相容矩陣。
4. 獨立離線企業包、名稱掃描、manifest／簽名／來源與必要通知。
5. 乾淨普通帳戶與目標端點試運行、完整逐項審核，全部有證據後才放行。

## 最新紅燈追蹤

- 已核對 `add2c7a` / `36133912085` 為成功：隔離候選的合成 fixture 通過。
- `57f660e` / `36134120053` 兩版本均失敗：真實引擎多輪歷史後執行
  `--snapshot-profile` 回傳 `E_LOCAL`，尚未進入候選建立與候選恢復。
  前置 picker 多輪及 restart continue 已通過，但不能據此宣稱候選恢復通過。
- 本次診斷切片保留失敗測試及所有快照檢查，加入中性 `E_SNAPSHOT_FS`，
  僅輸出固定操作階段、system/generic/other 錯誤域及數值碼，不輸出 OS 訊息或路徑。
  注入含私人路徑的 filesystem_error，先確認分類斷言紅燈，再實作分類通過；
  六組本機 C++、八项 Python 回歸通過。Windows 根因仍待此診斷 run 確認，
  長路徑只是待驗假設，未以猜測改動快照語義。

- `8384f04` / `36134887295`：兩版本均定位到 `copy-file: system: 3`。
  隨後 `6bc5c0b` / `36135222499` 加入不依賴引擎、來源有效且目的地超過
  260 字元的最小快照測試，兩版本均於 `create-directories: system: 206` 重現。
  確認快照新增層級存在 Windows 長路徑缺陷；真實引擎案例是否同因仍須修復後重跑。
- 長路徑修復使用快照／核驗／候選範圍內的 extended-length I/O 路徑，
  不更改 manifest 相對名稱、引擎 cwd 或對外回傳路徑，也不依赖機器登錄設定。
  同一來源／副本 hash、禁止覆寫及 symlink 檢查均保留；新增長路徑候選副本檢查。
  本機六組 C++、八項 Python 測試通過，Windows 結果待驗；此切片不等同
  整個產品所有長路徑／UNC／junction 已驗收。

- `7ed7122` / `36135633536` 尚未轉綠：既有短路徑 profile 測試在 copy-file
  回報 system:123。檢查發現 manifest 的 `/` 相對名稱加入 extended-length
  目的地後未轉成 Windows 原生分隔符；補在最終 I/O 邊界正規化分隔符，
  不更改 manifest 格式。既有短路徑及新長路徑測試全部保留，本機回歸通過，
  Windows 再驗中。

## 快照紅燈結案證據（仍非整體放行）

- 修復提交：`87f7065b1499b4c005460af526b0e901a4b6e074`。
- Windows CI：`36135890793`，兩版本 `2.1.221`、`2.1.282` 全部成功。
- 已查核兩個 job 的實際日誌，而非僅看綠色圖示：
  - `Checking snapshot destination beyond 260 characters` 之後 profile tests 通過。
  - `PASS: actual engine resumes isolated candidate history while active profile and backup remain byte-identical`。
  - SHA-256 清單由 Python hashlib 獨立核對通過。
  - 候選副本及篡改／缺檔／多檔／穿越拒絕案例通過。
- 真實引擎測試經歷原始歷史、picker 兩輪及 restart continue 後，再建立快照／候選。
  隔離候選恢復後的實際模型請求包含以上歷史標記，新輪次只落在候選；原 profile
  與整個 snapshot 保持逐位元組一致。API 仍為本機 fixture，不代表企業端點驗收。
- 前述 `57f660e` 真實 profile 快照紅燈，以及 `6bc5c0b` 最小長路徑紅燈已排除；
  既有短路徑回歸亦通過。未縮短工作區／暫存路徑、未刪除失敗案例、未跳過資料。
- 候選仍只有 `staged` 狀態，沒有產品級可信驗證回執、原子 active 指標或回滾流程。
  下一步必須補這些狀態與對應故障驗收；不能把本次成功視為 A15 全部完成。

## 產品級候選驗證切片（Windows 結果待驗）

- 新增 `--validate-profile ID --resume SESSION_ID`：雙 profile 排他鎖，先核對來源／
  候選清單，再用實際引擎於候選恢復指定歷史。提示不包含預期答案；回覆在記憶體
  比對首則使用者純文字，成功後才建立 verified 快照及 validation 回執。
- 回執綁定 engine version/SHA-256、adapter、workspace/session、來源及驗證快照，
  保存驗證後檔案 hash；scope 明列 single-session，不切換 active、不標成全面驗收。
  回執不是簽章／安全邊界；讀取端重新核對引擎、凍結快照及目前候選檔案。
- 本機 TDD 先出現 candidate.hpp／驗證介面缺失紅燈，再完成錯誤 probe 無回執、
  成功綁定、引擎不匹配及驗證後改檔拒絕測試；六組 C++、八項 Python 回歸通過。
- Windows fixture 新增公共命令真實恢復、答案未放入提示／終端／回執、獨立 SHA-256
  核對、錯誤答案拒絕及不重放、不啟用、來源不變斷言，尚待 CI 驗證。
- 使用／失敗重試限制見 `docs/candidate-validation.md`。原子啟用、完整歷史驗證範圍、
  來源已變更衝突、回滾等門檻仍保留，未由本切片取代。

- `16396cf` / `36137123613`：兩版本 public candidate verifier 均因
  `E_SNAPSHOT_INTEGRITY` 失敗。候選在驗證前持有新增 `frontend.lock`，卻誤用不可變
  snapshot 的完全相同檔案清單檢查；本機補同樣的 live lock fixture 後重現紅燈。
- 修正候選入口：將初始 manifest 與來源核對（僅候選 UUID 不同），再核對候選所有
  非 root frontend.lock 的檔案雜湊。不可變 snapshot 核验維持原本嚴格規則。
  測試同時確認候選任何其他多檔在呼叫引擎前拒絕，以及 snapshot 多出 lock 仍拒絕。
  本機六組 C++、八項 Python 通過；Windows 重驗待結果。

## 候選驗證 Windows 結果與回執讀取狀態檢查

- `5a9ca83` / `36137575727`：兩固定引擎全部步驟成功；已讀取兩個 job
  日誌，均確認 product candidate verifier 恢復真實歷史、凍結 SHA-256 證據、
  錯誤答案拒絕且不重放／不啟用的 PASS。先前 live lock 完整性誤判已排除。
- 這仍只是 single-session；不證明完整 profile 相容，不啟用候選。
- 後續 TDD 發現回執讀取端未核對 candidate metadata 的 schema/state：
  schema=2 的案例先在 rejectedMetadata 斷言失敗。補回與建立端一致的
  schema=1 / state=staged 要求；測試包含未知／null schema、active／null state，
  要求 E_CANDIDATE_DATA、回執不變，還原 metadata 後仍可核驗。
  此修正的 Windows 回歸結果另行記錄，不以此前 CI 代替。

## 來源變更衝突切片（Windows 待驗）

- 候選驗證新增必填 active profile，先核對非操作鎖檔案與來源快照完全一致，
  並在引擎 probe 後再次核對；不符回傳 E_SOURCE_CHANGED，不寫成功回執。
- TDD：先補快照後新增來源資料的案例，確認 sourceConflict 斷言紅燈，再實作
  請求前檢查；接著補 probe 期間外部寫入案例，確認 changedDuringProbe 紅燈，
  再實作 probe 後檢查。兩案均保留來源新資料，沒有自動合併或還原。
- 本機六組 C++、八項 Python、Python 語法及 diff 檢查通過。
- Windows 公共 CLI 新增來源已變更時退出 64、零 API 請求、候選／來源快照不變、
  來源新增檔保留及不啟用的案例，結果待 CI。正常候選驗證及錯誤答案案例保留。
- 未實作原子啟用；啟用時仍須重新核對來源，不能把本切片当作最終衝突保障。

- 前一個 metadata 核驗修正 `33cb808` / `36138276610`：兩引擎所有 Windows
  步驟成功（已核對 job 步驟及 run 終態）；此結果不包含本節來源衝突新增案例。

## 所有頂層會話驗證切片（Windows 待驗）

- `c64a067` / `36138791902` 兩版本全部成功；已查核兩份實際日誌的
  changed active source PASS，確認來源衝突在 API 前拒絕且保留所有資料。
- 新增 `--validate-profile ID --all-sessions`，與指定 resume 互斥。從所有原生
  project 頂層 JSONL 盤點，預檢完整記錄，逐一以原 cwd 恢復並核對原始歷史標記。
- 回執範圍明列 all-top-level-sessions，凍結整個驗證後 profile 並綁定所有
  session/workspace UUID；讀取端再次盤點，不能刪去某 session 後仍通過。
- 本機 TDD：先新增跨工作區盤點與全部會話介面測試（介面缺失紅燈），完成後通過；
  再新增 probe 偷換另一會話歷史標記測試，inventoryDrift 斷言先失敗，固定保存
  初始 marker 並在每次 probe 前與最後盤點比對後通過。
- 回歸測試另覆蓋損壞末行、重複 ID、第二次 probe 失敗不寫回執、刪減回執會話清單拒絕。
  本機六組 C++、八項 Python、Python 語法及 diff 檢查通過。
- Windows 新增真實引擎建立另一中文／空格工作區會話，兩個不同歷史標記各恰好
  一次恢復請求、兩工作區 UUID、完整 hashlib 回執及來源／快照不變的公共 CLI 案例。
  尚待 Windows CI 結果，不以本機 callback 代替引擎驗收。
- 不啟用候選；巢狀子代理及技能／MCP 相容、搬移與回退、完整交付／端點驗收仍未完成。

- `6b6165a` / `36139798291`：兩版本均在新增 all_result 成功斷言失敗，
  實際為退出 64 / E_CANDIDATE_HISTORY。測試共用了 legacy migration fixture，
  其中刻意有 old-session.jsonl／existing-session.jsonl 非 UUID、非 JSON 資料。
  本機重現這種資料被 inventory 拒絕。產品不能跳過它們後宣稱全部歷史通過。
- 修正測試分組，不放寬產品：原混合 profile 新增零 API、候選／來源／備份不變的
  明確拒絕案例；成功案例使用另一 data-dir，由真實引擎分別於兩工作區建立會話。
  不刪除原 fixture，並在成功案例結束仍核對它與原 snapshot 不變。
  六組 C++、八項 Python、語法及 diff 檢查通過；修正後 Windows 結果待驗。

## 全會話驗證差異診斷（未放行）

- `d535f54` / `36140223011` 已終止：2.1.221 全部成功；2.1.282 在 native
  runtime/resume 的 all_result 成功斷言失敗，退出 64 / E_CANDIDATE_HISTORY，
  後續 portable/tools 未執行。分離無效 legacy fixture 尚未解決全部問題。
- 暫停新增功能。失敗分支補結構化診斷，僅輸出 API 請求數、已知測試標記的
  命中布林值、會話／cwd 匹配布林值和記錄結構；不輸出提示、回覆或憑證。
  目的是區分預檢拒絕與真實恢復請求失敗，不改變任何成功斷言或產品檢查。
- 此為既有 Windows 紅燈的取證修改，非根因修復，須待新的 Windows 日誌定位。
- `272a201` / `36141049140` 的 2.1.282 job `108090694731` 已失敗。
  診斷證明預檢成功並發出一次 API 請求，歷史中包含第二工作區標記，兩份 transcript
  的 session/cwd 與原始標記均正確；probe 已寫入 transcript，但 API 最後一則
  message 不包含 probe。假 API 只檢查最後一則 message 的假設需進一步核實。
  補充每則 message 的 role、probe/marker 布林值與 content/block 型別以定位，
  仍不輸出內容，不修改驗證成功條件。

## 假 API 對原生尾隨 system 訊息的相容修正

- `52661f2` / `36141420238` 的 2.1.282 job `108091913353` 日誌顯示
  message 角色依序 user/system/assistant/user/system；驗證 probe 在倒數第二則
  user 訊息，最後的 system 不包含 probe。舊假 API 只看 messages[-1]，因此回覆
  resume-test-ok 而非歷史標記，產品嚴格核對後正確拒絕。
- TDD：抽出原有 fixture_answer（不改邏輯），新增尾隨 system 案例並親見紅燈
  resume-test-ok != legacy-resume-marker-7391；再改為定位最後一則 user，僅從
  它之前的歷史找唯一標記，仍檢查當前提示不得含答案。不是搜尋任意舊 probe。
- 六項 oracle 測試覆蓋尾隨 system、一般 probe、錯誤答案、答案洩漏、模糊歷史與
  後續一般 user turn；加既有測試共 14 項 Python 通過，語法及 diff 檢查通過。
- 本次只修測試 API，不改產品／驗收成功斷言。真實 Windows 雙版本回歸仍待驗，
  不以本機 oracle 測試宣稱整體歷史恢復或方案驗收完成。

## 全頂層會話驗證 Windows 通過證據

- 修復提交 `ff66afd`；Windows CI `36142006150` 終態 completed/success。
  2.1.282 job `108093860738`、2.1.221 job `108093861229` 全部步驟成功。
- 已逐份讀取兩個 job 的實際日誌，兩者均記錄：
  - all-session preflight 拒絕混合無效 legacy transcripts，沒有 API 請求或資料修改。
  - all-session candidate validation 恢復不同工作區的兩個真實引擎會話，
    完整 SHA256-bound receipt 核對成功，來源保持不變。
- 此前版本差異失敗由測試 API 錯把最後一則 system 當作當前 user turn 所致。
  修復沒有修改產品回覆核對、會話盤點、來源衝突或回執完整性要求。
- 本切片的 Windows 回歸門檻恢復通過，可以繼續後續實作；不表示完整 profile
  擴展語義相容，也不表示整份驗收矩陣通過。原子啟用／回滾、磁碟滿／中斷、
  細粒度並發、搬移與路徑邊界、故障分類、技能／MCP、獨立企業包及真實端點
  驗收仍待完成。
- 下一實作重點為原子 active profile 指標：須先在資料根取得協調鎖，再選取並
  鎖定 active profile；啟用前再次核對來源和完整驗證回執，單會話回執不得冒充
  全 profile 啟用依據。一般運行寫入後不能要求舊不可變回執仍與 live profile
  完全相同；必須區分啟用證據與運行中狀態。此段為待實作約束，不是已實作宣稱。

## 啟用前關卡切片（尚未接入 CLI／原子切換）

- 新增 VerifyCandidateActivation：重用完整回執／候選／凍結資料驗證，拒絕
  single-session 範圍；重驗來源快照及其 UUID 綁定，重新比對 active profile
  與來源快照，來源已寫入新資料時 E_SOURCE_CHANGED，保留新資料及回執。
- TDD：單會話拒絕案例先因介面不存在編譯失敗，加入 scope gate 後通過；再補
  驗證後來源寫入案例，activationSourceChanged 斷言先失敗，加入来源重驗後通過。
- 本機六組 C++、14 項 Python 與 diff 檢查通過。本切片尚未接入公開 CLI，沒有
  原子切換指標，也不宣稱啟用／回滾已完成。呼叫端仍須持有資料根協調鎖與兩個
  profile 鎖至原子提交結束；後續必須完成這些連接與 Windows 公共流程測試。

## Active profile 讀取與啟動協調鎖（Windows 待驗）

- 啟動端先持有 data/active-profile.lock，再讀指標並鎖定所選 profile；目前仍是
  粗粒度獨占，不宣稱同資料根多會話並發已實現。
- 缺少指標沿用 data/profile；既有指標損壞、未知 schema/adapter、引擎不符、
  非 UUID、回執身份不符或目標缺失均 E_ACTIVE_PROFILE，不默默退回舊 profile。
  候選各層目錄及指標／metadata／receipt 拒絕 symlink。
- 選取時核對已綁定的 engine/adapter/候選與驗證身份及全會話 scope；不把可變
  live profile 每次都與凍結檔案雜湊比較，避免正常新會話寫入後無法重啟。
- TDD：缺指標／損壞指標案例先因 API 缺失編譯紅燈；加入拒絕策略後通過；
  再加合法候選選取案例，先 E_ACTIVE_PROFILE 紅燈，完成身份核對後通過。
  另覆蓋 live 寫入後重選、錯誤 schema/engine/adapter/身份及缺失目標。
- 本機六組 C++、14 項 Python、語法與 diff 檢查通過。Windows 公共 CLI 新增
  損壞指標拒絕且不建立 fallback profile 案例，尚待驗證。
- 尚無公開啟用指令／原子指標寫入或回滾；不能手工建立指標代替完整啟用流程。

## 原子啟用命令切片（Windows 待驗）

- 新增 --activate-profile ID，拒絕混合其他操作。持有資料根／來源／候選鎖時
  呼叫完整啟用關卡；指標使用獨占建立 pending、寫入及 flush，再同目錄替換。
  Windows 使用 MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)，沒有第二個
  metadata 狀態寫入；指標為提交記錄。保留來源、備份及回執。
- TDD：啟用與重新選取案例先因 ActivateProfileCandidate 缺失編譯紅燈，
  完成後通過。另驗證 pending 已存在時拒絕並保留內容／無正式指標。
- 本機六組 C++、14 項 Python 通過；新增 Windows 公共案例包括來源變更拒絕、
  零 API 啟用、指標身份、两工作區重啟續接包含歷史、僅 live 候選寫入、
  原來源與備份不變。Windows 案例尚待 CI，不以本機 callback 代替真引擎證據。
- 尚未實作跨引擎回滾、pending 恢復、磁碟滿／強制中斷完整故障注入及並發細化；
  不宣稱斷電耐久性或完整遷移驗收。上一輪讀取指標 CI 36143176156 查核時仍在執行。

## 二次啟用／既有指標替換驗收補強

- `332ee7b` / `36142807782` 與 `a53703f` / `36143176156` 均已查核
  completed/success；分別是啟用關卡及指標讀取／協調鎖的 Windows 回歸結果。
- 本次不改產品：新增已啟用 profile 寫入後再備份、全會話驗證下一候選、替換
  既有 active 指標的回歸。pending 衝突時舊指標與資料完整保留；成功後保留
  第一個 active profile 的新增資料；重複啟用目前候選明確拒絕且指標不變。
- 本機 profile suite 通過。另在獨立 /tmp 副本做 mutation：讓 POSIX 指標提交
  遇到既有指標就拒絕（模擬只能首次建立的錯誤），新增測試確實以
  E_ACTIVATION_WRITE 失敗；正式程式未修改。證明案例能攔住不支援替換的回歸。
- Windows 公共流程加入同樣二次啟用／pending／重複啟用案例，尚待 CI；
  測試手工清除的是自身建立的 synthetic pending，不代表產品已有恢復指令。
- 原子啟用首輪 CI 36143635951 查核時兩版本 native/resume 與 portable 已成功，
  tools 等後續步驟仍在執行，未提前記為整輪通過。

## 批准等待取消的歷史落盤前置條件修正

- `37d2923` / `36144060613` 舊引擎 job `108100655531` 的 native/resume、
  portable 已通過，工具步驟失敗於取消後的「沒有 transcript」断言；真實批准／
  拒絕／取消本身與兩個 process-tree 案例已列印 PASS。不能標示整輪通過。
- 測試原先看到 console 批准提示即取消，未先確認原生引擎非同步歷史寫入。
  修正 recovery 案例的前置條件：在等待批准時輪詢完整換行 JSONL user 記錄，
  有限等待，未落盤或進程先退出明確失敗；不創造／修復任何 transcript。
  取消碼、目標零副作用、重啟列出歷史與逐位元組保留的原斷言保持不變。
- TDD：新增 helper 公開行為測試先因缺少介面失敗，再實作並通過；另外覆蓋
  空檔、未完成行、非 user／損壞 JSON 不能冒充落盤證據。17 項 Python 通過。
  真正 Windows 時序是否解決仍需本次 CI 驗證；不宣稱未落盤歷史可恢復。
- pending 歸檔仍為工作區未完成的紅燈測試，未納入此修復提交，停止新功能直至
  CI 回復。全量驗收仍未完成。

## 中斷啟用 pending 顯式歸檔（Windows 待驗）

- `eb95749` / `36144714978` 已確認兩個 Windows 引擎 job 全部成功，批准取消
  的測試前置條件修正通過。另已讀取 `36144060613` 兩 job 的具體 PASS 訊息，
  確認全會話驗證、首次與二次原子啟用案例通過；該整輪仍為 failure，未改記。
- 新增 --archive-activation-pending：資料根鎖內、選取 active profile 前執行，
  原樣移動 pending 到獨立 UUID 歸檔目錄，不解析、不切換、不重放；缺失檔案、
  非一般檔案、symlink 及碰撞拒絕。失敗不清理證據。重試啟用仍走完整關卡。
- TDD：原樣歸檔案例先因缺少 API 編譯失敗；最小實作通過後，碰撞案例再次
  斷言失敗，加入 UUID／存在性／檔案類型／碰撞關卡後通過。六組 C++ 與
  17 項 Python 通過，語法及 diff 檢查通過。
- Windows 公共流程不再刪除 synthetic pending，改用真正歸檔命令；新增損壞
  正式指標、二進位截斷 pending、無 API／不建立 profile 與混合參數拒絕案例。
  這些新增 Windows 案例待 CI；不是磁碟滿／強制中斷／跨引擎回滾全部完成。

## Windows 真實指標替換失敗故障案例（新增案例待驗）

- `7befb21` / `36145555370` 已完成 success；已讀取兩個 job 的日誌，均有
  pending 原始 bytes 歸檔、損壞正式指標不變／不建立 profile、二次啟用 PASS。
- 新增真正的 OS 故障案例：以 Windows handle 允許正式指標讀写但不分享 delete，
  再呼叫公開 --activate-profile，要求 E_ACTIVATION_WRITE；不是預先放入 pending
  來模擬提交失敗，也沒有增加可由使用者啟用的產品故障開關。
- 案例檢查舊指標逐位元組不變、來源／候選／快照／verified 副本不變、零 API；
  真正寫出的 pending 綁定預期候選，未歸檔重試仍拒絕；釋鎖後公開歸檔保持
  pending bytes，再完整驗證啟用成功。fault handle 即使斷言失敗也釋放。
- TDD：fixture helper 測試先因缺 API 紅燈，再加入取得／釋放 handle 實作通過；
  本機 19 項 Python、語法與 diff 檢查通過。Windows 行為以後續 run 為準，
  本機 Mock 只驗證 fixture 控制流程，不證明 OS rename 故障已驗收。
- 磁碟滿、真正進程強制中斷、跨引擎回退及完整企業端點驗收仍未完成。

## 啟用 I/O 錯誤分類修正（Windows 待驗）

- `62b842e` / `36146154154` completed/success；已逐一讀取兩個 Windows job
  的真實 pointer replacement failure PASS，確認正式指標保留、pending 歸檔
  及驗證後重試的實際故障流程通過，不只是 helper Mock。
- 原先 CreateFile/open 的所有失敗都錯報 E_ACTIVATION_PENDING。現在只有
  已存在的 pending（Windows FILE_EXISTS/ALREADY_EXISTS、POSIX EEXIST）
  使用此碼；其他建立失敗使用 E_ACTIVATION_WRITE。Windows close 失敗也
  不再提交正式指標。候選完整驗證仍先於內部提交原語，沒有公開繞過入口。
- TDD：抽取提交原語先測首次寫入與覆寫；新增不存在父目錄案例在舊分類上
  斷言失敗，修正後通過。另覆蓋既有 pending bytes 不變、真實 rename 目標
  阻擋時 pending 保留且原目標不變。本機六組 C++、19 項 Python 全部通過。
- 新增 C++ 案例由 Windows build 原有六 suite 執行；本次 Windows 結果待 CI。
  父目錄缺失及 rename 失敗不能冒充磁碟滿或斷電驗收，這些缺口仍保留。

## 跨引擎回退前資料保全切片（Windows 待驗）

- `d14c98f` / `36146756580` 已確認 completed/success，I/O 分類修正通過
  兩版本 Windows 回歸。
- 發現舊引擎不只不能運行新版 active，也無法透過公開命令備份現有資料。
  新增備份專用選取流程：仍完整核對指標、回執身份與記錄的引擎，僅不要求
  備份程式內嵌的引擎相同；不啟動引擎。一般執行仍要求引擎完全相同。
  --snapshot-profile 改為獨占命令，禁止與其他操作／prompt／模型參數混用。
- TDD：备份解析介面測試先編譯失敗，再完成實作；同時保持不相容引擎的一般
  運行拒絕，以及各種損壞／身份不符指標的備份拒絕。本機六組 C++、19 項
  Python、Python 語法和 diff 檢查通過。
- 新 Windows 案例在真實會話資料副本中建立 synthetic 引擎身份差異：公開備份
  無憑證成功、逐檔 SHA256/size 一致、來源和指標不變；一般運行仍拒絕，
  回執身份損壞後備份也拒絕且無新快照。此案例待 CI，且 synthetic 身份
  差異不能證明兩個真實引擎間的資料相容性或完整回退驗收。
- 仍欠相容快照選擇、目標引擎全會話驗證、保留新資料後的回退提交及故障驗收；
  此切片僅解除「不能先保全資料」的障礙，不縮減完整設計要求。

## 顯式回退準備核心（尚未接公開 CLI，Windows 待驗）

- `6a61b77` / `36147386390` 已查核 completed/success，兩版本 Windows
  建置、resume、portable 與實際工具步驟均成功。
- 新增內部 PrepareProfileRollback：呼叫方須持有資料根與目前 profile 鎖；
  核驗指定舊快照，先完整快照保全現有 active，再建立獨立 rollback-candidates，
  記錄來源／保全快照、原指標與目標引擎。準備不更改正式指標、不啟動引擎，
  不與目前歷史合併；完成文件不是相容性批准。
- TDD：保全較新資料案例先因缺介面編譯失敗；加入實作後通過。目的地碰撞、
  快照父目錄 symlink 各自先斷言失敗，再增加寫入前拒絕關卡後通過。
  補驗非法／不存在來源不建立備份或候選；來源／目前資料／指標保持不變。
- 本機六組 C++、19 項 Python 通過；本次新增核心 Windows 案例待 CI。
  本切片尚無公開回退命令，仍欠 CLI、目標引擎全會話驗證及原子回退提交，
  不能宣稱完整回退或整體驗收完成。中途失敗保留資料，不自動刪除或重放。

## 公開回退準備命令（Windows 綠燈待驗）

- 前一核心提交 `af44625` / `36148280161` 已查核 completed/success。
- 測試先行提交 `4b8cf4f` / `36148513947` 兩版本皆在公開
  --prepare-rollback 測試失敗；已讀取兩 job 日誌，確定原因為
  E_UNSUPPORTED_OPTION，而非既有會話回歸。
- 接上 --prepare-rollback SNAPSHOT_ID：保持資料根／active profile 排他鎖，
  採備份專用 active 選取，呼叫準備核心後回傳 JSON 計畫；禁止混用其他操作，
  不要求憑證、不啟動引擎。候選留在獨立 rollback-candidates。
- Windows 測試使用真實會話加 synthetic 引擎身份差異，檢查明確選舊快照、
  最新 active 的保全快照、原指標與來源不變、無 API、無相容驗證回執。
  這不是兩個真實版本間的相容性驗收。本機六組 C++、19 項 Python 通過；
  新 launcher 公開行為須由下一輪 Windows CI 證明，完整回退仍未完成。

### 公開回退準備 Windows 結果

- `154b5eb` / `36148979959` completed/success。兩版本 `2.1.221`
  （job `108117151311`）、`2.1.282`（job `108117151367`）均完成全部步驟。
- 已分別讀取日誌，兩者都有 `PASS: explicit rollback preparation preserves
  latest active data and pointer, isolates old snapshot, and needs no API`。
  portable 混合參數拒絕、既有 native/resume 及真實工具回歸亦成功。
- 此結果只證明公開準備階段；尚未提供回退驗證／提交，不能等同完整跨引擎
  回退或企業驗收放行。

## 回退全會話驗證核心（未接公開命令，Windows 待驗）

- 新增 ValidateProfileRollback，僅接受準備計畫指定的目標引擎；重用完整會話
  盤點及歷史標記驗證，探測對象是舊快照候選，不將目前 active 錯當成舊來源。
- 每次探測前後與完成時核對原指標、準備計畫、來源／保全快照及目前 active。
  active 必須仍等於保全快照，否則停止；不接受已過時準備，不丟棄新寫入。
  成功回執將全會話 validation 綁定完整計畫；使用獨立 rollback-verified 與
  rollback-validation.json，尚無提交入口，不能由普通啟用命令使用。
- TDD：先缺介面編譯失敗；目標引擎不符、準備後 active 變動、父目錄連結、
  既有 pending 證據分別先斷言失敗，再完成拒絕關卡。補驗探測中 active／指標／
  計畫變動停止、錯誤歷史只探測一次不重放、兩個會話都驗證且正式指標不變。
- 本機六組 C++、19 項 Python、diff 檢查通過。C++ probe 是受控測試回呼，
  不是真實引擎相容性證據；本次 Windows 核心回歸待 CI。公開命令接線、真正
  目標引擎恢復與原子回退提交仍需實作／驗證，整體狀態維持未放行。

## 公開回退驗證接線（Windows 待驗）

- `99d2181` / `36150304809` 已確認 completed/success。
- `c32b34f` / `36150504265` 為本次測試先行紅燈；已讀取兩版本 job 日誌，
  皆因 --validate-rollback 尚未實作回報 E_UNSUPPORTED_OPTION 失敗。
- 接上 --validate-rollback ID，沿用實際引擎 recovery probe、停用工具、限制
  一輪，依原工作區核驗全部會話；只允許額外 --model，不接受單會話驗證。
  持有資料根、active 及候選鎖；management 選取允許目前 active 屬於另一引擎，
  驗證核心仍強制 targetEngine 身份相符。
- 新 Windows 案例要求無憑證拒絕、兩個真實會話請求都含歷史標記、回執綁定
  計畫／引擎／完整 session 集合、備份與 active 不變、重跑零 API 並拒絕。
  目前 active 的引擎差異仍是 synthetic；不是兩個真實版本間的相容性證據。
- 本機六組 C++、19 項 Python、語法及 diff 檢查通過；公開命令 Windows 綠燈
  待下一輪 CI，原子回退提交及完整驗收仍未完成。

### 公開回退驗證 Windows 結果

- `f276ff6` / `36151042542` completed/success，兩版本所有步驟通過。
  已讀取 `2.1.221` job `108124081588` 與 `2.1.282` job `108124081921`
  日誌，均有 public rollback verifier 的全會話恢復、資料保留、拒絕重放 PASS。
- 此處是真實內嵌引擎及真實會話，模型回覆仍由本機假 API 提供；目前 active
  引擎差異仍是 synthetic。不得稱為真實跨版本回退或企業 gateway 驗收完成。
- 原子回退提交尚未實作，其餘完整驗收門檻仍保留。

## 原子回退提交核心（未接公開命令，Windows 待驗）

- 新增 ActivateProfileRollback，呼叫方須持有資料根／active／候選鎖。提交前核對
  完整回退計畫與回執、目標引擎、全會話範圍、凍結候選內容、來源／保全快照，
  原指標及目前 active 必須仍與準備時一致。沿用既有原子指標提交原語，不重放 API。
- 回退指標使用 schema 2 與 profileKind=rollback，綁定保全快照。舊前端只接受
  schema 1，因而明確拒絕，不會把回退候選當普通候選。新前端仍接受原 schema 1，
  回退啟用後可重啟／備份；允許 active 日常新增資料，不拿凍結驗證內容限制正常寫入。
- TDD：缺介面、驗證後新寫入、回執綁定不符、損壞來源、schema 防舊前端誤選，
  各先失敗再實作通過。補驗 pending 保留／歸檔後重試、重複提交拒絕、未知種類／
  保全 ID／回執損壞拒絕，舊 active 與保全備份保持不變。
- 本機六組 C++、19 項 Python 通過；schema 2 修改後 profile suite 再通過。
  此為核心測試，不是公開 Windows 回退驗收。CLI 接線、實際重啟恢復、Windows
  回退替換故障與真實跨引擎組合尚待完成；整體未放行。


## 公開回退提交接線（Windows 待驗）

- `c104467` / `36152210697` 已確認 completed/success，兩版本核心回歸成功。
- 測試先行 `ad8f5b4` / `36152774491` 兩版本均失敗；已讀取日誌，
  確認 --activate-rollback 回報 E_UNSUPPORTED_OPTION，非既有驗證流程回歸。
- 新公開命令接入既有原子回退核心，管理選取允許目前 active 引擎不同；目標引擎
  仍必須符合計畫。命令獨占操作，拒絕重複參數與非 UUID，持有三層鎖，無 API。
- Windows 測試要求 schema 2 指標、重啟可列出所有原會話、舊 active／保全與
  來源逐位元組不變。公開結果待下一輪 CI；本機六組 C++、19 項 Python 通過。
- 實際 resume、新寫入拒絕、Windows 替換失敗／歸檔重試及真實跨版本組合仍待
  公開端到端驗收；完整方案維持未放行。

### 公開回退提交重啟紅燈診斷

- `396128d` / `36153539976` 兩版本失敗。實際日誌顯示提交及資料保全斷言已過，
  重啟 --sessions 返回成功但輸出 No saved sessions for this workspace，不能稱驗收通過。
- 修正測試的工作區範圍：兩個真實會話屬於不同 cwd，應分別核對各自列表，而非
  要求單一工作區列出兩者；空列表仍然不會通過。
- 新增不依賴引擎的最小長路徑會話發現案例：父目錄短於 260、有效 transcript
  超過 260，要求正常找到歷史。本機通過，Windows 待驗；長路徑目前只是待確認
  根因，不提前修改產品路徑行為或放寬空列表斷言。

### 會話長路徑修復（Windows 待驗）

- `070f669` / `36154095260` 兩版本均在最小測試 longSessions.size() == 1
  斷言失敗，確認 ListSessions 對超過 260 字元的有效 transcript 漏列。
- 將既有快照 extended-length I/O 轉換提取為共用 NativeIoPath，快照入口保持
  相容；會話發現從轉換後的 projects 路徑列舉及讀取。不改引擎 cwd、環境、
  transcript 的 cwd 或工作區身份，不使用 8.3 別名。
- 本機六組 C++、19 項 Python 與 diff 檢查通過；Windows 最小測試及原始公開
  回退重啟案例仍須下一輪 CI 確認，不能先標記已修復或完整驗收通過。

### 公開回退提交與長路徑 Windows 結果

- `ec29bd3` / `36154511355` completed/success。兩版本全部步驟成功；已讀取
  `2.1.282` job `108135669384` 與 `2.1.221` job `108135669988` 日誌，
  均完成長路徑 session 最小測試及 public rollback activation 重啟列表 PASS。
- 此結果證明原先空列表案例已修復；不是所有工具長路徑或真正跨引擎相容性證據。
- 接續補公開端到端回歸：驗證後新寫入拒絕、真實 Windows 禁止 DELETE sharing
  導致指標替換失敗、pending 保留及明確歸檔後重試、重複提交拒絕，並在兩個
  原工作區以原 session ID 恢復，核對實際引擎請求中的原歷史標記。所有舊 active、
  snapshots 與 frozen checkpoint 必須不變；無憑證提交階段零 API。
- 新增案例本機 Python 語法、19 項回歸及 diff 檢查通過；Windows 待下一輪 run。
  故障注入為真實檔案替換失敗，不等同磁碟滿或斷電。整體仍未放行。

### 回退故障與實際續接 Windows 結果／真正跨版本驗收接線

- `7af0fef` / `36155207524` completed/success，兩版本所有步驟成功。
  已讀取 `2.1.282` job `108137937932` 與 `2.1.221` job `108137938108`，
  均有 rollback rejects newer writes / Windows replacement failure / resumes
  both real sessions PASS。此證據不涵蓋磁碟滿或斷電。
- 新增依賴兩個成功建置 artifacts 的獨立 Windows cross-version job，直接使用
  真正 `2.1.221` 與 `2.1.282` 封裝檔，不改寫版本 metadata、指標或驗證回執。
- 案例：舊版建立會話及快照 → 新版全會話驗證、啟用並寫入新輪次 → 舊版準備、
  驗證及明確回退 → 舊版恢復原 session。核對引擎版本及不同 SHA、原歷史標記、
  新版輪次確實落盤且完整保全，舊快照、舊 profile、新版 active 前後不變。
- 本機 Python 語法、19 項回歸及 diff 檢查通過；真正跨版本結果尚待 Windows。
  模型回覆仍使用本機假 API，不能替代企業 gateway／乾淨端點驗收；整體未放行。

### 跨版本驗收前置 fixture 修復

- `2592c60` / `36156016696`：兩個單版本 job 成功，cross-version job
  `108142325697` 失敗，尚未進入真正兩引擎生命週期。已讀取日誌，失敗位於
  mixed-profile preflight 的零 API／候選不變斷言。
- 原 verify 依賴 test-windows.ps1 額外建立的無效 JSONL 名稱；獨立 cross-version
  job 只呼叫 prepare，缺少該前置條件，因此測試進入引擎驗證而非 preflight 拒絕。
- TDD 新增 standalone preparation 測試先以 0 != 1 失敗，再令 prepare 自行用
  exclusive-create 建立獨立無效歷史 fixture。不改產品或放寬原拒絕斷言；不覆寫
  既有檔案。20 項 Python、語法與 diff 檢查通過，Windows 重跑待驗。

### 真正跨版本回退通過／程式搬移驗收補強

- `13847b5` / `36156944431` completed/success：單版本 `2.1.221`
  job `108143678763`、`2.1.282` job `108143678939` 及 cross-version
  job `108145290363` 均成功。已讀取跨版本日誌，確認 actual engines
  2.1.221 -> 2.1.282 -> 2.1.221 preserve original history and newer data PASS。
- 此證據是兩個真實引擎二進位的建立／升級驗證／新版寫入／舊版回退／恢復；
  不是 synthetic metadata。模型仍為本機 API fixture，企業端點未因此通過。
- 下一個測試增量：回退後新版實際引擎拒絕舊版 active 且資料不變；將舊版 exe
  真正搬至空格／中文新目錄，沿用明確 data-dir 與原工作區，核對 UUID 不變、
  會話可列出與實際恢復包含回退後輪次，搬移後程式目錄不產生動態資料。
- 搬移／不相容新案例本機 20 項 Python、語法及 diff 檢查通過，Windows 待驗。
  工作區本身搬移、資料根搬移、全部路徑邊界與其餘驗收門檻仍未完成。

### 程式目錄搬移案例修正

- `938c85f` / `36157769257` 兩個單版本 job 成功；cross-version
  `108147961938` 的跨版本回退、搬移後 UUID、列表與實際續接斷言已過，
  最後程式目錄逐位元組不變斷言失敗。
- 檢查測試發現僅搬 exe，未搬既有 runtime。PrepareRuntime 會於新程式目錄
  重新解出原始 engine.exe 及建立 prepare.lock；設計允許版本化 runtime，禁止的
  是 session/temp 動態資料。將案例改為真正搬移整個程式目錄，不排除任何新增
  檔案，也不放寬逐位元組比對。搬移前額外核對 engine SHA 與回退計畫相符。
- 本機 20 項 Python、語法、diff 檢查通過；Windows 搬移結果待下一輪確認。
  單 exe 首次解出時的完整交付邊界檢查仍屬其餘包／資料分離驗收範圍。

### 完整程式目錄搬移 Windows 驗收結果

- `a264098` / `36158567029` completed/success；兩個單版本 job
  `108149106830`（2.1.282）、`108149107257`（2.1.221）及 cross-version
  `108150662899` 全部成功。已讀取 cross-version 實際日誌，不僅依賴綠色狀態。
- 執行環境：Windows Server 2025 Datacenter，10.0.26100；runner image
  windows-2025-vs2026 / 20260907.229.1。日誌於 2026-09-25T16:11:35Z
  記錄真正兩版本回退，以及 incompatible actual engine / moved program PASS。
- 證明完整程式目錄（含已核對 SHA 的既有 runtime）搬到中文／空格路徑後，
  原工作區 UUID 不變、原 session 可列出並真正續接，請求包含回退後歷史；
  程式檔案逐位元組不變，舊 profile、新版資料與快照保全，不相容新版拒絕啟動。
- 本案例沿用原工作區及原外置 data-dir；不涵蓋工作區搬移、資料根搬移、
  單 exe 首次解出交付邊界、磁碟滿／斷電。模型 API 仍為本機 fixture，
  不代表乾淨普通帳戶或企業 gateway 已驗收。整體維持實施中、未放行。


### 外置資料根搬移 Windows 驗收結果

- `ee9e8b5` / `36159427257` completed/success；單版本 jobs
  `108152014483`（2.1.221）、`108152014543`（2.1.282）及 cross-version
  `108153682839` 全部通過。已讀取實際日誌，2026-09-25T16:20:07Z
  明確輸出 relocated external data root PASS，前面的跨版本回退與程式搬移亦 PASS。
- 案例先搬移整個資料根，不改 native history 或驗證回執，僅更新公開的
  CCODE_DATA_DIR 設定。原工作區 UUID 不變；原 session 能列出並由實際舊版
  引擎恢復，請求包含原始、回退後及程式搬移後歷史，不包含新版保全分支輪次。
- 舊 data-dir 不被重建，程式目錄不變，來源快照、舊 profile、新版資料與
  保全快照逐位元組不變，active pointer 不變。新增測試第一次 Windows 即通過，
  此增量沒有產品修復，不宣稱存在產品 RED→GREEN。
- 上方 A14/A15/D10 已依累積結果回填，未刪除剩餘驗收門檻。
  工作區本身搬移仍需明確遷移；資料根搬移成功不能代替它。整體仍未放行。

### 超長 profile 的身份與索引 TDD 修復

- `35607dd` / `36160376359` 兩版本在新增超過 260 字元 profile 測試失敗；
  首次僅有未處理異常終止碼，未據此猜測具體錯誤。`4cd0f32` 增加測試診斷，
  `36160859413` 兩版本明確報告 ResolveWorkspace 的 create_directories 路徑找不到。
- 測試 fixture 已透過 NativeIoPath 建立該目錄，故不是不存在的輸入；產品登記檔
  操作尚未使用 Windows extended path。修復在 registry 與 session-index 的 IO
  邊界統一使用既有 NativeIoPath，不改 workspace key、儲存的身份或原生歷史。
- 本機六組 C++、20 項 Python 及 diff 檢查通過；Windows 修復結果仍待下一輪。
  此案例覆蓋長 profile 的登記檔建立／重讀、歷史發現及刪除索引後重建，
  不代表全部工具或長工作區 cwd 已驗收，整體仍未放行。

### 長 profile 身份與索引 Windows GREEN

- `237eb50` / `36161251653` completed/success，兩版本單測與整合以及跨版本
  job 均通過。已讀取實際日誌：`108158077223`（2.1.221）及 `108158077702`
  （2.1.282）均在 Checking workspace identity and rebuilt index beyond 260
  characters 後輸出 session discovery passed，先前失敗案例已完成 Windows RED→GREEN。
- cross-version `108159847314` 亦完成資料根搬移續接 PASS。修復未改寫歷史內容，
  也未放寬超長路徑斷言。此證據仍不涵蓋超長工作區 cwd、全工具鏈、UNC 或企業端點。

### 超長 cwd 平台限制：未通過且保留獨立放行 gate

- `cdaa5a3` / `36162165020` 兩版本在 Python CreateProcess 啟動產品之前
  回傳 WinError 267；不是工具本身的 RED。`468053f` / `36162600975` 用
  extended-path 格式傳同一 cwd 仍於同一處失敗，已讀取兩版本實際日誌。
- Microsoft SetCurrentDirectory 文件明確記載超過 MAX_PATH 的目前目錄會
  使 CreateProcessW 失敗（2026-09-26 查閱）：
  https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
- 不以縮短 cwd、8.3 alias、改寫工作區或跳過斷言冒充完整成功。超長 cwd 的
  六工具驗收移至獨立 long-workspace matrix job，仍保留失敗退出碼，無
  continue-on-error；依賴一般測試 artifacts，與 cross-version 並行，使平台
  限制不再遮住一般工具、權限、取消、恢復與跨版本回歸。整個 workflow 仍會
  因此 gate 未過而失敗，不代表完整方案已完成或准予放行。
- 超长檔案／profile 可用與超長 cwd 可啟動是不同範圍。A08 完整邊界仍未完成，
  需要可驗證且不違反原生 cwd 設計的解法，或使用者明確批准調整支援範圍；
  本次沒有自行縮小原始目標。

### 獨立長 cwd gate 結果與 A10 事件大小邊界修復

- `d8dc753` / `36163109763` 整體 failure：一般兩版本 jobs
  `108164254187`、`108164254413` 及 cross-version `108165776014` 成功；
  long-workspace `108165775980`、`108165776154` 均失敗。已讀取失敗日誌，
  仍為產品啟動前 WinError 267。沒有把整體 CI 標為成功，該門檻保持未完成。
- A10 新增每行恰好 16 MiB 的合法 JSON 事件，兩個事件合併單次 Feed 或跨次
  Feed 都必須等價通過。原實作以 pending 加整批 bytes 的總長度判斷，測試先
  明確以 E_EVENT_LIMIT 失敗；改為逐行、配置記憶體前檢查剩餘容量後本機通過。
- 單行超限（一次或分片）、超限後不可復活仍拒絕；換行分隔符不計入 JSON
  行內容上限。修復沒有提高 16 MiB 單事件限制，也不改 JSON/schema 錯誤分類。
- 本機六組 C++、20 項 Python 及 diff 檢查通過；Windows 回歸待下一輪。
  這只完成事件大小分片語義的本機 RED→GREEN，不替代斷流／gateway 端到端驗收。

### A10 大小邊界修復 Windows 回歸確認

- `3be31ae` / `36163996525`：兩個一般版本 jobs `108167176324`（2.1.221）
  與 `108167176553`（2.1.282）成功；已讀取實際日誌，均輸出 frontend event
  tests passed，包含每行 16 MiB 邊界、合併／分片等價、超限後不可復活測試。
- cross-version `108169153506` 成功，日誌確認資料根搬移後真實續接回歸 PASS。
- 整體 workflow 仍為 failure：long-workspace jobs `108169153499` 與
  `108169153585` 均 WinError 267，已核對日誌。不得將一般回歸成功寫成全量成功。
- A10 仍缺實際串流中斷／gateway 故障端到端證據；A08 超長 cwd 仍未完成。

### A18 401 故障：確認重複請求並固定禁止自動重試

- `14338ad` / `36164951835` 兩版本 gateway rejection 超過 60 秒失敗。
  `bc0d68a` / `36165902295` 補安全診斷後仍失敗；兩版本均收到 7 次
  `/v1/messages`，stdout/stderr 皆為 0 bytes。不能宣稱遮罩或錯誤分類已通過，
  也未僅因逾時就擅自增加時間或刪除不重放斷言。
- 2026-09-26 查閱官方環境變量文件
  https://code.claude.com/docs/en/env-vars.md ：提供 CLAUDE_CODE_MAX_RETRIES、
  CLAUDE_CODE_RETRY_WATCHDOG、CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK。
  子進程環境在別名展開後固定為 0、0、1，避免繼承設定重新啟用自動重試／後備請求。
- 環境測試先在 MAX_RETRIES=0 斷言 RED，修復後六組本機 C++ 與 20 項 Python
  GREEN；包含呼叫者用中性別名嘗試覆寫的案例。真實兩版本引擎的 401 行為仍待
  Windows CI 證明，設定存在不等於引擎端到端已合格。整體仍未放行。

### A18/A19 不重放通過至下一斷言，修復錯誤事件內容外洩

- `1389662` / `36166994169` 兩版本不再逾時，依測試斷言順序，非零退出及
  僅一次模型請求已通過；均在 Gateway details leaked to terminal 失敗。
  不能把這次結果寫成 gateway 驗收成功。
- 核對官方 SDK 的 AssistantMessage.error 型別（authentication_failed 等）：
  https://github.com/anthropics/claude-agent-sdk-python/blob/main/src/claude_agent_sdk/types.py
  前端先識別結構化 error，不把錯誤事件 content 當作一般模型文字輸出；
  authentication_failed 對應 E_GATEWAY_AUTH，其餘未知錯誤使用固定 E_ENGINE。
  不解析或轉印原始 gateway 訊息，後續 success result 也不得清除既有失敗狀態。
- 新測試先因原始錯誤文字外洩而 RED，修復後六組 C++、20 項 Python GREEN。
  401 整合斷言加強為必須出現 E_GATEWAY_AUTH，Windows 真實事件相容及遮罩
  仍待下一輪驗證。本機單測不替代端到端，整體仍未放行。

### HTTP 401 真實引擎驗收 GREEN；追加 429 分類案例

- `7afab7c` / `36167769785` 兩版本 jobs `108179588875`（2.1.282）及
  `108179589344`（2.1.221）成功；已讀取實際日誌，均輸出 HTTP 401 PASS。
  斷言涵蓋非零退出、一次模型請求、E_GATEWAY_AUTH、假 token／gateway 私有
  標記不出現在終端、工作區無寫入及程式區無 JSONL。只證明本機 fixture 的 401，
  不代表真實企業 gateway 或所有敏感內容儲存邊界已驗收。
- cross-version `108181267182` 成功，實際日誌確認舊→新→舊及程式／資料根
  搬移續接仍通過。long-workspace `108181267322`、`108181267328` 仍為
  WinError 267，整體 workflow failure，未放行。
- 下一個垂直切片為 HTTP 429：沿用完整斷言，fixture 回 rate_limit_error 並帶
  Retry-After: 1，必須只發一次請求且輸出 E_GATEWAY_RATE_LIMIT。前端分類
  測試先 RED，加入固定中性分類後六組 C++、20 項 Python、語法與 diff 檢查
  GREEN。兩版本實際 429 行為待 Windows CI，沒有提前標為通過。

### HTTP 429 Windows GREEN；啟動不完整工具串流驗收

- `09a8a43` / `36168642003` 兩版本 jobs `108182481596`、`108182482137`
  成功；已讀取實際日誌確認 HTTP 401 與 429 均 PASS，後者含 Retry-After: 1、
  一次請求、E_GATEWAY_RATE_LIMIT、非零退出、無工作區寫入及終端私有內容遮罩。
- cross-version `108184292666` 成功；long-workspace `108184292755`、
  `108184292775` 仍 WinError 267。整體 workflow failure，不代表全量放行。
- A10/A18 下一案例使用真實引擎且明確批准 Write；fixture 發出 tool_use 開始及
  未完成的 input_json_delta 後，在承諾的 Content-Length 結束之前關閉連線。
  測試要求確實送出截斷資料、非零退出、一次模型請求、中性錯誤、無工作區
  寫入及終端遮罩，不能因未批准工具而假通過。此輪僅加入驗收測試，沒有產品
  修復，不宣稱 RED→GREEN；Windows 實際結果待確認，細分斷流分類仍未完成。

### A10/A18 截斷工具串流 Windows GREEN，補正常 HTTP EOF 邊界

- `1cafc70` / `36169518496` 的兩版本 jobs `108185358073`（2.1.282）與
  `108185358385`（2.1.221）成功；已讀取實際日誌，401、429 與 truncated tool
  stream 均 PASS。已批准 Write、未完成參數、Content-Length 尚未滿足即斷線
  的案例確認一次模型請求、非零退出、中性錯誤、無寫入與終端私有內容遮罩。
- cross-version `108187637442` 成功；long-workspace `108187637534`、
  `108187637631` 仍 WinError 267。整體 failure，A08 與其他放行門檻仍保留。
- 新增不同故障邊界：HTTP Content-Length 精確且正常 EOF，但 SSE 缺少
  content_block_stop、message_delta、message_stop，工具參數 JSON 仍未完成。
  保持完全相同的批准、不重放、非零退出及無寫入斷言，以區分 HTTP 傳輸失敗
  與協定未終止。此輪僅加驗收案例，結果待 Windows CI，不宣稱產品 RED→GREEN。

### 正常 HTTP EOF 驗收 RED，另有舊版 lifecycle 回歸失敗

- `ec4ac61` / `36170484184` failure。已讀取失敗日誌：2.1.282 job
  `108188530319` 的 401、429、HTTP 截斷案例通過，新增正常 EOF 案例在
  模型請求數不等於 1 的斷言失敗；不能把傳輸截斷成功擴大成所有斷流成功。
- 2.1.221 job `108188529965` 在既有 crash lifecycle 等待 child.pid 失敗，
  前端仍活著；清理 communicate 又逾時，frontend.lock 被占用。gateway 被跳過。
  這不是新 EOF 案例的結果，也尚無證據可定性為暫時性 runner 問題。
- 補充模型請求的純結構診斷：與第一份 body 是否相同、stream 布林值、歷史
  訊息／tool_use／tool_result／工具錯誤數與工作區是否改變，不輸出原始內容。
  建置成功且未取消時獨立執行 gateway 步驟，前一步失敗仍保留 workflow 失敗，
  無 continue-on-error，不放寬任何驗收斷言。下一輪須分別追查兩個失敗。

### 正常 HTTP EOF 重放已定位；獨立驗收不再被 gateway 紅燈遮蔽

- 已重新查核 `0b4eecd` / `36171418367` 的狀態與失敗日誌：整體 failure；
  2.1.221 job `108191604052` 成功，2.1.282 job `108191604315` 的正常 HTTP
  EOF 案例失敗。兩次模型請求 body 完全相同，均 stream=true、兩則歷史訊息、
  沒有 tool_use／tool_result／工具錯誤；工作區沒有改變。這證明新版重放一次，
  不是工具執行後的正常下一輪。401、429、HTTP 傳輸截斷案例仍通過。
- 本輪兩版本既有工具／lifecycle 步驟成功，但不能因此結案前輪 crash 清理逾時；
  根因仍未確認。cross-version 與 long-workspace 因 needs:test 失敗而 skipped，
  沒有新的通過證據；先前長 cwd 的 WinError 267 仍是未解門檻。
- CI 增加獨立驗收調度：建置成功且未取消即上傳「built」產物，不再命名為
  「tested」；缺失產物明確失敗。跨版本與長工作區在上游測試失敗時仍執行，
  取消時不啟動。保留所有測試斷言及整體 failure，不使用 continue-on-error。
  若建置失敗導致缺產物，下游下載同樣失敗，不製造虛假驗收通過。
- 新 workflow 契約測試先 RED（產物上傳受預設 success 條件限制），調整後本機
  21 項 Python GREEN、diff 檢查通過。這只驗證調度設定；實際 Windows job
  調度與各驗收結果仍須新 run 證明，沒有宣稱已修復 EOF 重放或長工作區。

### EOF 重放根因線索與結構化 retry 中止切片

- 唯讀檢查本機原始 2.1.282 Windows 負載（SHA256
  `fc0e3af017705624b9e1bce913f72761864ff994804514da1f5e41380fca4484`）中的
  引擎程式碼：StreamTruncated 分支選用獨立的固定一次重試上限，而非一般
  MAX_RETRIES 設定；該分支在等待重試前 yield retry 事件。這與先前兩次相同
  請求一致，但沒有修改負載，也不將內部實作名稱當作支援介面。
- 已取得官方 https://code.claude.com/docs/en/headless.md 的 Handle API retries
  章節，確認 system/api_retry 是結構化重試前事件。文件也明示 apiKeyHelper
  某些認證重試可能不發事件；因此事件中止不是所有重試路徑的完整保證。
- 新增每個 byte 分片及 retry 後同批 success 不得恢復的測試，先 RED（前端
  原先忽略該事件），再令 EventReader 回 E_GATEWAY_RETRY，沿用 RunTurn 的
  TerminateJobObject 及非零退出；只顯示固定「automatic retry refused」，
  不透傳上游 error、狀態文字或路徑。保留環境中原有三個禁止重試／fallback 設定。
- 本機六組 C++、21 項 Python、diff 檢查 GREEN。原有 Windows gateway fixture
  的一次模型請求、非零退出、無工作區寫入及遮罩斷言完全不變；是否在重試發送
  前及時終止必須由新一輪 Windows 證明，不因 unit test 通過而提前結案。

### 結構化 retry 中止：兩版本 EOF 端到端 GREEN

- `0c561a7` / `36173166133` 已結束，實際日誌確認 jobs `108197367591`
  （2.1.282）、`108197367872`（2.1.221）的 HTTP 401、429、傳輸截斷及正常
  HTTP EOF 未完成工具串流四案例均 PASS：非零退出、一次模型請求、無工作區
  寫入及終端私有內容遮罩。這次新版 EOF 重放案例由 RED 轉 GREEN；保留原斷言。
- cross-version `108199232593` 成功，實際日誌確認舊→新→舊、程式及外置
  資料根搬移後續接均 PASS。long-workspace `108199232524`、`108199232686`
  仍 WinError 267；整體 failure，未放行。事件處理速度及無事件的上游重試仍
  不能以本案例推廣成所有網路故障的保證。
- 前一輪 `42c71b8` / `36172601161` 已完成：新版仍有同樣的兩次請求紅燈，
  但跨版本 job 成功，兩個 long-workspace job 都實際執行並報 WinError 267。
  證明獨立驗收調度已生效，並未以跳過其他門檻遮蔽整體失敗。

### A10/A11 補完整工具參數、缺 block 終止事件的端到端邊界

- 前輪 EOF 案例的 input_json_delta 本身不是完整 JSON，不能因此宣稱已覆蓋
  「參數已完整但 block／message 未終止」。新增正常 HTTP EOF 案例：Write
  已明確批准、參數是完整合法 JSON，但不送 content_block_stop、message_delta
  或 message_stop；仍要求一次模型請求、非零退出、中性錯誤、無工作區寫入及遮罩。
- fixture 事件產生器測試先 RED（缺少介面），最小實作後驗證完整 JSON 案例
  確實缺三個終止事件，原有不完整 JSON 案例仍無法解析。23 項 Python、語法及
  diff 檢查 GREEN。這是 fixture 的紅綠測試，不是產品端到端通過；Windows
  結果待新 run。沒有改產品邏輯或放寬任何既有驗收標準。

### 完整 JSON、缺 block 終止事件：兩版本 Windows GREEN

- `30c751e` / `36174265807` 已完成，已讀取實際日誌。2.1.282 job
  `108200943088`、2.1.221 job `108200943572` 的新增「complete tool JSON
  without block termination at clean HTTP EOF」均 PASS：已批准 Write、參數
  JSON 完整合法但缺 block 終止，仍非零退出、只發一次模型請求、不寫入工作區，
  並保留終端私有內容遮罩。原有四個 gateway 案例亦 PASS。
- cross-version `108202893467` 成功，實際舊→新→舊及外置資料根搬移續接
  PASS。long-workspace `108202893221`、`108202893301` 仍 WinError 267，
  整體 failure，未放行。此結果僅補上述 block 終止邊界；不代表所有擴展、
  網路、資料安全或目標企業端點驗收已完成。

### A08/A13 大小寫與 junction：新增獨立真實工具／歷史驗收

- 增加同一實體工作區的大小寫別名與 NTFS junction 兩個場景，不使用它們替代
  long-workspace 案例。先從實體路徑建立 UUID，再透過別名執行完整六工具鏈；
  從實體目錄核對 Write/Edit 及 Bash 結果，兩種拼法應得到同一 UUID，registry
  不得增加或改寫身份；兩種拼法列出相同真實 session，列表不得改動歷史檔。
- 最後從實體路徑 --continue，檢查實際模型請求包含原提示及 Read/Bash 的歷史
  標記，只允許一次請求；不以 UUID 或列表相同冒充引擎已恢復完整上下文。
- 加獨立必需 CI 步驟，建置成功且未取消即執行。workflow 契約測試先因步驟
  缺失 RED，加入後本機 24 項 Python、語法及 diff 檢查 GREEN。此輪只增加
  驗收案例，沒有產品修復；Windows 真實工具、junction 與大小寫結果待驗。


### 大小寫／junction：兩版本真實工具、UUID 及續接 GREEN

- `92f3e3a` / `36175453668` 已完成，實際日誌確認 2.1.282 job
  `108204825723`、2.1.221 job `108204826011` 均有 case alias 與 junction
  alias PASS：六工具真正操作同一實體檔案、UUID 相同、registry 不變、兩路徑
  列出同一 session、列表不改寫原始歷史，回實體路徑 --continue 的模型請求
  包含原提示及工具結果歷史。不等於工作區本身搬移已驗收。
- cross-version `108206719485` 成功，已核實舊→新→舊 PASS。兩個
  long-workspace jobs `108206719510`、`108206719532` 仍是 WinError 267，
  整體 failure，未放行。沒有用 junction 別名把長 cwd 門檻改成短 cwd。

### A13 損壞末行不可被第一筆有效 user 隱藏

- session discovery 原本找到第一筆有效 user 即停止，且跳過無效 JSON；新增
  有效 user 後接截斷 JSON、非 object JSON、損壞完整行的測試，先在拒絕斷言
  RED。改成繼續掃描並以 E_SESSION_DATA 拒絕，保留第一筆摘要；讀取失敗與
  超限行亦不再靜默當成正常歷史。原測試曾明確容許截斷末行，現改為獨立
  正常歷史 fixture 加上述拒絕案例，不再把損壞檔列為正常 discovered。
- 三案例逐位元組確認來源不變；新增 Windows 公共 --sessions 驗收，要求
  exit 64、只有中性錯誤、無私有歷史輸出及原檔不變。本機六組 C++、24 項
  Python、語法與 diff 檢查通過；Windows 結果待 CI。
- 此切片採保守整次列表拒絕，尚非逐會話 unavailable 展示、引擎相容性判定
  或完整損壞恢復流程。長 cwd、企業端點等完整門檻保持未放行。

### 損壞末行拒絕：兩版本 Windows 驗收已核實

- `ed3b03b` / `36176839996` 已完成，已讀實際日誌，不是僅看 job 狀態。
  2.1.221 `108209395776`、2.1.282 `108209396030` 均明確輸出
  `PASS: malformed history tails are classified without disclosure or transcript changes`。
  主測試全部成功；既有真實多輪續接、別名工具及 gateway 案例未被跳過。
- cross-version `108211831458` 成功，日誌核實真正舊→新→舊、外置資料根
  搬移、歷史續接及保全資料。這些結果不替代逐會話 unavailable 狀態或
  目標普通帳戶／企業 gateway 的完整驗收。
- long-workspace `108211831496`、`108211831561` 均失敗；實際 traceback
  在 Python subprocess 的 `_winapi.CreateProcess` 回報 WinError 267，
  尚未啟動前端。整體 workflow failure，完整方案仍未放行。

### A13 逐會話不可用狀態與續接拒絕

- 延續上一輪損壞檢查：若已由原生有效 user 記錄確認工作區歸屬，再遇損壞，
  公共列表保留 ID 並標示 unavailable／E_SESSION_DATA，摘要替換成中性文字；
  可重建索引保存同一狀態。不能確認歸屬的損壞仍整次拒絕，不猜測或修復來源。
- --resume ID、--continue 選到已知不可用會話時拒絕；picker 不改變目前選擇，
  清楚回報不可用。continue 不靜默跳到較舊會話。discovered 仍不是已驗證相容。
- TDD：混合正常／損壞原生歷史測試先因缺可用性介面 RED，實作後驗證正常
  會話仍存在、損壞會話拒絕、索引可用性及原始 bytes 不變。六組 C++、24 項
  Python、語法／diff 本機通過；Windows 新增列表、指定／最新續接拒絕及
  picker 拒絕不發 API、健康會話實際兩輪上下文續接案例，結果待 CI。
- 公共 --sessions 對已知歸屬損壞的預期從 exit 64 改為成功列出不可用狀態；
  不是略過損壞或放寬續接門檻。嚴格 discovery 原測試與未知身份錯誤仍保留。


### 逐會話不可用：兩版本真實續接驗收 GREEN

- `bcd18a5` / `36178387814` 已完成，已讀實際日誌。2.1.221 job
  `108214488984`、2.1.282 job `108214489181` 均確認 malformed history
  保留 unavailable 列表狀態，--resume ID／--continue 拒絕且來源不變。
- 同兩版本真實引擎 fixture 確認 picker 不選中損壞會話、不送 API；同一
  profile 的健康會話仍可選中、實際兩輪模型請求包含舊上下文，損壞檔 bytes
  完全不變。不是只以列表或索引存在當作續接成功。
- cross-version `108216564763` 成功，確認舊→新→舊與外置資料根搬移續接。
  long-workspace `108216564886`、`108216564905` 的實際日誌仍為
  WinError 267；整體 failure，未放行。未重啟或縮減該驗收門檻。

### A18 gateway 不可達：新增實際連線拒絕驗收

- 新增保留但不 listen 的 loopback TCP 埠 fixture，保持 socket 所有權到引擎
  結束，避免先關閉 listener 後被其他程式搶佔。Windows 設 exclusive address
  use；實際 runner 先確認 TCP 連線不成功，再啟動原始引擎。
- 要求 60 秒內非零退出、中性 E_ 診斷、假 token／私人 prompt 不出現在終端、
  工作區沒有寫入及程式目錄沒有 JSONL。僅使用假憑證、本機端點，不觸及企業服務。
- fixture TDD 先因缺介面 RED，實作後本機 25 項 Python 通過；socket bind
  需沙箱外授權，已授權重跑而非跳過。語法與 diff 檢查通過。Windows 結果待驗。
- 此案例不計算連線重試次數，也不把泛用中性錯誤當作已完成細分網路錯誤分類；
  TLS、DNS、逾時／斷線分類、實際過期憑證與企業端點完整門檻仍保留。

### 不可達 gateway 結果及獨立工具驗收調度

- `0d8e312` / `36179544424` 已完成，整體 failure。兩版本 gateway 驗收
  通過，實際不可達端點案例通過；cross-version `108221421992` 成功。
- 2.1.282 的 portable 清理因 ccode.exe 被占用回報 WinError 32，原工作流
  因此前一步失敗跳過工具驗收。2.1.221 工具驗收在權限拒絕案例未等到本次
  提示，隨後 kill/wait 逾時並發生 frontend.lock 占用；根因尚未確認。
- long-workspace 兩版本仍在 CreateProcess 回報 WinError 267，未啟動前端。
  不以其他案例通過替代這些失敗，也不將未解釋的占用歸因於 runner。
- 新增調度回歸測試，先確認缺少獨立條件時 RED，再讓工具步驟在 build 成功
  且未取消時獨立執行。保留失敗門檻、不加 continue-on-error；這只避免驗收
  被遮蔽，並未修復進程占用。完整本機 Python 26 項 GREEN，新 Windows 待驗。

### 獨立調度後 Windows 結果（d256d68）

- 同一工作 `36181272230` 已等待至終態，沒有因觀察逾時重新執行。已讀實際
  日誌：主測試 2.1.221 `108223961557`、2.1.282 `108223961768` 均成功，
  真實 console allow／deny／cancel、工具樹 cancel／crash、不可達 gateway
  均明確 PASS。cross-version `108225861518` 亦確認實際舊→新→舊保留歷史。
- 本輪沒有重現前輪 ccode.exe／frontend.lock 占用，但未找到根因、未作產品
  修復，不可據單次成功宣稱該問題已解決。調度修正的目的仍只是防止漏跑。
- 長工作目錄 `108225861554`、`108225861702` 仍在啟動時 WinError 267；
  整體 workflow failure。完整驗收、企業端點、交付與其他未完成項均未放行。

### A06 原生重命名與失敗後來源保全測試

- 新增共用 rename 驗收 oracle：真實 rename 後來源消失、目標 stat/read/list
  一致；原名 stat 明確 ENOENT。再對不存在父目錄 rename，要求 ENOENT 且
  來源 bytes／列表不變、無目標殘留，不使用跨平台不同的覆寫規則作假設。
- TDD：先新增正常原生 I/O 與「虛假成功但未搬移」故障注入測試，缺介面 RED；
  實作後 Node／Bun 各 2 項 GREEN，Python 26 項 GREEN。Windows fixture
  複製同一 oracle，後續 cmd 子進程、原始引擎內建 Grep／Glob 改讀重命名檔。
- 本機 Bun 為 1.3.13，CI 固定 1.3.0；Windows 兩版本執行結果待驗。本項是
  原生 runtime／搜尋鏈證據，不宣稱已完成模型驅動重命名、全部故障範圍或
  長 cwd 支援；A06 其餘門檻保持。

### A06 Windows 原生重命名結果已核實

- `05750fe` / `36182355211` 已等待至終態並讀取實際日誌。2.1.221 job
  `108227498888`、2.1.282 job `108227499101` 均明確 PASS：原生 rename
  與缺父目錄失敗保全、cmd 子進程讀取新名、原始引擎內建 Grep／Glob。
  主測試兩版本均成功，cross-version `108229406750` 真實舊→新→舊通過。
- long-workspace `108229406783`、`108229406791` 實際 traceback 仍為
  WinError 267；整體 failure。未將原生測試擴大解讀為模型驅動重命名、
  長 cwd 或目標企業端點驗收完成。完整門檻不變。

### A06 實際引擎跨工具重命名鏈（待 Windows）

- 在既有六工具之後加入真實 Bash mv → Read 新名 → Edit 新名 → Grep／Glob
  新名 → 缺父目錄 mv 失敗後 cat；要求每項工具成功完成其驗證命令、回傳
  對應 marker，舊名消失、新名 bytes 精確符合編輯結果、失敗目的地不存在。
  此鏈亦接入大小寫與 junction alias 案例，物理路徑驗證同一重命名結果。
- TDD：新增檔案結果 oracle 測試，先因缺介面 RED，再以實際臨時檔覆蓋
  舊名殘留、內容破壞、目的地殘留與健康情境；Python 27 項、Node 2 項 GREEN。
- 本輪只修改驗收 fixture，沒有改動產品或略過既有門檻。Windows 結果待驗；
  模型回覆是本機 deterministic API，不宣稱真實模型／企業端點已驗收。

### 跨工具重命名 Windows 結果及 fixture 換行修正

- `8908d32` / `36183450145` 已完成並核對日誌。2.1.282 `108231071150`、
  2.1.221 `108231071625` 的工具及別名步驟均明確 PASS：Bash rename、
  Read/Edit/Grep/Glob 新路徑、失敗後 edited bytes 保全。各版本一般、case、
  junction 共三案例通過。cross-version `108232898352` 成功。
- 主測試兩版本在新增 Python oracle 測試第 27 行失敗，尚未進入原生 runtime
  與 portable 驗收：write_text 的 Windows 換行轉換與精確 LF bytes 斷言衝突。
  本機強制 text writer newline=CRLF 重現同一 RED；改用固定 write_bytes 後
  相同重現環境 GREEN，完整 Python 27 項 GREEN。額外明確測試 CRLF 應拒絕，
  不放寬產品／oracle 的 bytes 要求。Windows 修正後回歸待驗。
- long-workspace `108232898299`、`108232898430` 仍 WinError 267，整體
  failure。不得將通過的工具步驟等同本輪全部原生與 portable 案例通過。

### 固定 bytes Windows 回歸及 console 停滯再次出現

- `7d579ce` / `36184360358` 已等待至終態並讀取日誌。兩版本 Python 27 項、
  native runtime／resume、portable 步驟通過；換行 fixture 修正已在 Windows
  驗證。兩版本一般／case／junction 的跨工具 rename 均明確 PASS。
- 2.1.221 `108234057952` 主測試全部成功；2.1.282 `108234058157` 在首個
  permission allow 案例失敗：No real permission prompt，poll=None，兩條
  captured pipe 空白，console 空白；kill 後 wait 15 秒仍逾時，frontend.lock
  WinError 32。與較早 deny 案例不同，此次沒有先前提示留屏；不得把停滯僅
  歸咎於舊 console 畫面。現有證據不足以確定卡在哪個啟動／清理階段。
- cross-version `108237019701` 實際舊→新→舊成功。long-workspace 兩版本
  仍在 CreateProcess WinError 267；整體 failure。下一步需蒐集進程／執行緒
  等待證據以定位 console 停滯，而不是加重試或將此類失敗忽略。

### 權限停滯：加入有界、數值白名單的進程診斷

- 在等待 permission prompt 逾時及 kill 後等待逾時、進程尚可觀察時取得
  前端／後代 PID、parent PID、thread ID、state、wait reason 數值快照。
  讀取有 10 秒上限，不收集命令列／環境／路徑／提示，不輸出原始 stderr。
  診斷失敗僅輸出 unavailable；原本 assertion／timeout 仍拋出，不重啟或忽略。
- TDD：先新增白名單與異常不洩漏測試，缺介面 RED；實作後本機 30 項測試
  中 29 通過、1 個 Windows 真實進程／執行緒測試因平台跳過。該真實測試
  在 Windows 必須 captured 且當前進程有 threads；不以 mock 宣稱 API 已驗證。
- 這是為定位間歇停滯補證據，不是停滯修復；失敗時才記錄快照，不引入
  自動重試、放寬超時或 continue-on-error。Windows 實際收集與根因仍待驗。

### 進程診斷 Windows 真實 API 驗證

- `8a2ac80` / `36185827200` 已等待至終態並核對日誌。兩版本 Python 30 項
  通過，Windows 專用 test_windows_process_diagnostics_capture_actual_live_threads
  明確 ok：確實取得存活進程及執行緒，不是只驗 mock 白名單輸出。
- 兩版本主測試、console allow／deny／cancel 與 cross-version 均成功；本轮
  未重現停滯，沒有 DIAGNOSTIC timeout 快照。因此只證明收集器在正常 Windows
  進程可用，不能證明停滯根因或宣稱已修復。後續遇到停滯仍須讀取現場證據。
- 兩版本 long-workspace 的實際日誌仍為 CreateProcess WinError 267；整體
  failure，全部未完成門檻保留，未用成功主測試替代完整驗收。

### A11 重複工具宣告不可被靜默去重

- 原前端將 init.tools 直接插入 set，重複名稱被靜默接受。新增每個 transport
  byte 分片點的重复宣告測試，先在缺拒絕行為時 RED；改為插入失敗立即回報
  中性 E_TOOL_DUPLICATE_NAME，後續合法 init 亦只能得到 E_PROTOCOL_FAILED。
- 六組本機 C++ 通過；Python 30 項中 29 通過、1 個 Windows API 測試因平台
  跳過，diff 檢查通過。Windows 兩個原始引擎與擴展相容結果仍待驗。
- 此切片強化前端事件契約，不改寫工具名稱、不代猜工具，也不宣稱能撤銷
  引擎內部已執行的操作。A11 完整狀態轉移與核准擴展驗收仍保留。

### 重複工具宣告 Windows 回歸及初始化狀態鎖定

- `be9f26a` / `36186966481` 已完成並核對實際日誌。2.1.221
  `108242619045`、2.1.282 `108242619637` 主測試全部成功；兩版本 Python
  30 項、原生 runtime/resume、前端、實際工具、別名及 gateway 案例通過。
  cross-version `108244755470` 成功。兩個 long-workspace job
  `108244755484`、`108244755485` 仍明確 WinError 267，整體 failure。
  本輪沒有權限停滯 DIAGNOSTIC，不能據此宣稱間歇停滯已修復。
- 下一個 A11 狀態缺口：重複 init 原先可清空並替換工具登記，同時改寫 session。
  新增原樣重複及不同工具／session 的 init，在每個 byte 分片點拒絕，保持原
  session，後續輸入不能恢復已失敗串流。先在 ExpectError RED，再於任何
  session／registry 變更前拒絕為 E_PROTOCOL_ORDER，測試 GREEN。
- 另外以獨立 RED/GREEN 驗證 result 完成後不能首次初始化。六組 C++、Node
  2 項通過；Python 30 項中 29 通過、1 項 Windows 專用測試跳過。新增狀態
  限制的 Windows 實際引擎回歸待驗；不宣稱前端能撤銷引擎已執行的工具。

### A13 回合會話身份不可被事件改寫

- `RunTurn` 原本使用未綁定身份的 EventReader，任何帶 session_id 的事件都會
  改寫 reader.session，最後可能成為下一回合的 resume 目標。現在回合首次
  宣告後固定身份；若呼叫者正在 resume，建構 reader 時即綁定預期 session。
  不符時 E_SESSION_MISMATCH、保留原身份、不接受成功 result，後續輸入失敗。
- TDD：先以每個 byte 分片點注入不同 session 的 result，ExpectError RED，
  最小修復後 GREEN；再加入預綁 resume 身份測試，缺建構介面 RED，實作後
  GREEN。同身份事件／正常 result 與未指定身份的新回合維持相容。
- 六組 C++、Node 2 項通過；Python 30 項中 29 通過、1 Windows 專用項跳過。
  本輪兩個實際引擎的 resume／跨版本相容性尚待 Windows 驗證，不以 parser
  單測宣稱完整歷史驗收，不聲稱可以撤回引擎在拒絕之前的內部操作。

### 初始化狀態鎖定 Windows 回歸

- `7599787` / `36188226673` 已終態並核對日誌。2.1.282 `108246766197`、
  2.1.221 `108246766470` 主測試全部成功；cross-version `108249397945`
  明確顯示實際 2.1.221 → 2.1.282 → 2.1.221 通過。兩個 long-workspace
  `108249397978`、`108249398006` 實際仍 WinError 267；整體 failure。
  無權限停滯診斷快照，不宣稱該間歇問題已解決。
- 這份結果只覆蓋初始化鎖定提交；下一個會話身份固定提交 `ad17981` 的
  Windows 驗收另由 `36188662312` 執行，不沿用前一提交結果替代。

### 會話身份固定 Windows 實際引擎驗證

- `ad17981` / `36188662312` 已終態並讀取日誌。兩版本主測試
  `108248186903`、`108248187169` 全部成功；Python 30 項通過，兩輪歷史
  picker 續接有明確 PASS。cross-version `108250151373` 明確驗證原始
  2.1.221 → 2.1.282 → 2.1.221 歷史／新版資料保全，綁定 resume 身份未
  破壞這些已覆蓋流程。實際工具、別名、console 權限及 gateway 步驟也通過。
- long-workspace `108250151422`、`108250151552` 實際仍 WinError 267，
  整體 failure。沒有 DIAGNOSTIC 停滯快照，未解根因及其餘完整驗收缺口保留。
  本機 API fixture 不替代企業端點、真實模型或普通帳戶完整驗收。

### 工作區身份更新中斷證據不可覆寫

- ResolveWorkspace 原本以 trunc 開啟既有 workspaces.json.new，新增工作區會
  靜默覆寫上次中斷的身份資料。現在保留該檔及已提交 registry，回報中性
  E_WORKSPACE_PENDING；既有已提交身份仍可讀取，不自動採用未完成內容。
- TDD 先建立實際 pending 檔並新增工作區，pendingRejected 斷言 RED；最小
  加入存在檢查後 GREEN。測試逐 bytes 確認兩份檔案不變；明確將 pending
  另存後重試成功，原有 UUID 不變，保存的中斷內容仍在。原 symlink 拒絕保留。
- 六組 C++ 通過，Python 30 項中 29 通過、1 Windows 專用測試跳過。另加入
  Windows 真實 ccode.exe --workspace-id 驗收：既有身份可讀、未知工作區
  拒絕且不洩漏 pending 內容、手動保存後可重試。該實際命令驗收待 CI。
- 這是特定中斷殘留的保全，不等同斷電／磁碟滿全矩陣完成；也沒有新增自動
  合併或宣稱已完成工作區搬移遷移。保存動作目前由操作員明確執行。

### 工作區中斷登記的公開封存命令

- 新增 --archive-workspace-pending，持有既有 data-root／profile 排他鎖後，
  將 workspaces.json.new 原樣封存至 profile/workspace-recovery/<UUID>/pending.json。
  只輸出 recovery UUID；不解析／合併 pending，不修改已提交 registry，不需
  API 憑證或啟動引擎。拒絕与正常回合、其他維護動作及引擎參數混用。
- TDD：先缺封存介面 RED，實作後正常封存／再登記 GREEN；另以缺檔、無效
  recovery ID、既存目的地及非普通檔案注入 RED，再加入中性分類拒絕使 GREEN。
  原身份、pending 與已封存 bytes 均檢查保全；不覆寫既存恢復證據。
- 六組 C++、Python 29 通過／1 Windows 專用跳過。Windows portable 驗收
  已改為呼叫公開命令而非測試自行 rename，並覆蓋混用參數及再次封存缺檔。
  真實 Windows 結果待 CI，不把本機 helper 測試当成完整產品驗收。

### 工作區中斷保全與公開封存的 Windows 結果

- `e481a49` / `36189757081` 已終態且讀取實際日誌：兩版本主測試
  `108251767278`、`108251767715` 及 cross-version `108253582998` 成功；
  portable 明確驗證 pending bytes 保全、已提交身份可讀與手動保存後重試。
  long-workspace `108253582981`、`108253583093` 仍 WinError 267。
- `c17b873` / `36190191804` 已終態且讀取實際日誌：主測試 `108253196864`、
  `108253197164` 成功；portable 相同案例已改為實際公開封存命令，包含
  無憑證封存、缺檔及參數衝突拒絕。cross-version `108255080146` 明確通過
  實際兩版本升級回退。long-workspace `108255080193`、`108255080231`
  仍 WinError 267，兩輪整體皆 failure。兩輪無 DIAGNOSTIC 停滯快照；
  不宣稱間歇停滯已修復，也不將這兩個切片等同完整中斷／災難恢復驗收。

### A12 權限請求先驗證 RPC 外層

- 權限入口原先只要求存在 id，未驗證 jsonrpc 即可呼叫批准 callback。現在
  只允許 jsonrpc="2.0" 及可關聯的整數／字串 ID 進入批准流程；其他請求
  回報 -32600、不產生 permission decision，無效 ID 不原樣回傳私人物件。
  無 ID 的通知仍不呼叫批准。這是入口驗證，不是間歇 console 停滯修復。
- TDD：先用錯誤／缺失版本重現 callback 被呼叫的 RED，再加入驗證使 GREEN；
  再以 null／布林／小數／陣列／物件 ID 重現 RED，最小修復後 GREEN。
  正常整數／字串 ID 各只批准一次，保留原工具參數；正常拒絕測試保留。
- 六組 C++ 通過；Python 29 通過、1 Windows 專用跳過。portable 新增
  真實權限 worker 的批次無效請求案例及不回顯私人 ID 斷言。Windows 真實
  引擎 allow／deny／cancel 相容性與新 worker 案例待本輪 CI，不能沿用舊結果。

### RPC 外層驗證實際結果與診斷收集器失敗

- `66d1836` / `36191269730` 已終態，已讀取完整日誌。舊版主測試
  `108256681816` 成功，真實 permission worker 拒絕 malformed RPC 案例明確
  PASS；新版 `108256681548` 在 Python Windows 真實診斷測試回傳 unavailable，
  而非 captured，導致 native runtime/resume 步驟失敗及 portable 步驟跳過。
  因此新版 malformed RPC 案例本輪沒有執行，不能以舊版結果代替。
- 兩版本獨立 tools 步驟的真實 console allow／deny／cancel 均 PASS，別名與
  gateway 步驟成功；cross-version `108258585034` 升級／回退及資料根搬移
  續接明確 PASS。long-workspace `108258584972`、`108258584986` 日誌仍為
  WinError 267。整體 failure，未放行。
- 原收集器把啟動、逾時、非零退出和格式錯誤都合併為 unavailable，現有日誌
  不足以確認此次失敗根因；不能稱為 runner 波動或 console 停滯重現。
  TDD 新增固定白名單原因分類斷言先 RED，再最小分類處理 GREEN；不輸出
  原始命令／stdout／stderr／例外字串。Windows 真實測試失敗時顯示安全快照，
  仍要求 captured 及真實 threads，不加重試、不提高 10 秒上限。
- 本機 Python 30 項：29 通過、1 Windows 專用跳過，diff 檢查通過。
  本次只改善根因可觀測性，不宣稱收集器或 console 間歇故障已修復。
  同步 A06／A09–13／A15／A18 摘要與既有追加證據；未移除任何剩餘放行條件。

### A18 不信任 TLS 憑證：新增真實握手與引擎驗收門檻

- 新增僅綁定 127.0.0.1 的 TLS fixture，憑證有正確 IP SAN，附公開測試專用
  私鑰；不修改系統或引擎信任庫。TDD 先缺 fixture 介面 RED，再以真實 TLS
  驗證預設信任必須拒絕，明確只在測試 client 信任該憑證後可收到 HTTP 503，
  排除伺服器本身不可用、主機名不符或握手根本未執行的假陽性。
- Windows 實際引擎新增要求：觀察到 TLS 拒絕、零 HTTP 請求、非零退出、
  E_GATEWAY_TLS 專屬中性分類、無工作區寫入及終端 token／提示洩漏。
  置於既有 gateway 案例之後，不因新門檻失敗遮蔽原有案例。
- 本機 Python 31 項：30 通過、1 Windows 專用跳過；只代表 fixture 有效。
  兩個原始引擎的這個新門檻尚未執行，現有通用 E_ 不算 TLS 分類驗收完成。
  不信任憑證只涵蓋 TLS 的一種故障，TLS 全矩陣、DNS、過期憑證及企業端點
  仍欠。不得把 fixture 專用公開私鑰用於交付或部署。

### RPC 及診斷分類 Windows 回歸證據

- `b0ad018` / `36192125436` 已等待至終態並核對完整日誌。主測試
  `108259468069`（2.1.221）、`108259468397`（2.1.282）全部成功；兩版本
  真實 Windows 進程／執行緒 API 測試及 malformed RPC worker 案例明確通過，
  真實 console allow／deny／cancel 亦均 PASS。
- cross-version `108261109035` 舊→新→舊歷史及新版資料保全 PASS。
  long-workspace `108261108989`、`108261109011` 實際仍 WinError 267，
  整體 failure。沒有 DIAGNOSTIC timeout 快照，不能藉此聲稱前輪 unavailable
  或先前 console 停滯的根因已確定／已修復。新增 TLS 門檻不在本 run 範圍。

### TLS 新門檻被 GitHub Actions 帳戶限制阻擋，未執行

- `454abc2` / `36192868765` 立即終態 failure，所有 jobs 都沒有 steps；
  `gh run view --log-failed` 回報 log not found。已查閱新版 job
  `108261875239` 的 check-run annotation：帳戶近期付款失敗或需要提高
  spending limit，job 未啟動。annotation 沒有區分這兩種原因，不能自行推斷。
- 這是 Actions 帳戶可用性阻擋，不是 TLS 測試 RED／GREEN；所有新 Windows
  案例均未執行。既有本機 TLS fixture GREEN 不替代兩版本引擎驗收。
  需帳戶擁有者確認 Billing & plans 並恢復 runner 可用性；不自行更改計費、
  不盲目 rerun，也不削減測試範圍。完整目標仍實施中、未放行。


### A02 交付名稱掃描器：本機實作完成，實際交付驗收仍欠

- `scripts/ccode/package_audit.py` 提供唯讀 CLI，分別掃描解包目錄與 ZIP，
  檢查大小寫不敏感的檔名、目錄、公開文字及 JSON 解碼後的鍵值。
  禁用名称需明確提供，不自行假設企業政策；ZIP 交付檔名亦檢查。
- 原始 `.exe`／`.dll` 內容只可透過精確 opaque 清單排除，檔名不豁免；
  要求 MZ 標頭但不冒稱完整 PE 或簽名驗證。必要通知不可排除、刪除或改寫。
  拒絕連結／reparse point；ZIP 拒絕跳出路徑、連結及大小寫碰撞，不解壓執行。
- 前續實作按 RED→GREEN 推進；本輪審查既有未提交實作及重新執行驗證：
  名稱掃描 11 項通過；完整 Python 42 項，41 通過、1 Windows 專用跳過。
  包含原始負載／通知位元組不變、UTF-16、JSON escape、錯誤 ZIP 中性報錯、
  CLI 退出碼及 ZIP／解包內容一致性案例。此結果不是 Windows 實際交付證據。
- 操作及限制見 `docs/verification/package-name-audit.md`：不掃描 ZIP 註解、
  PE metadata／簽章發行者或任意設定語言 escape；不抵抗並發修改，沒有
  hash 綁定或簽名 manifest。報告必須存於候選外，不能自掃描而污染結果。
- A02 仍未完成：需核准名稱清單、實際獨立交付 ZIP 及解包後掃描、更新殘留、
  必要通知核實，並關聯包雜湊、版本、OS／帳戶與完整驗收 metadata。
  未改動 Windows 門檻，亦未因 Actions 帳戶限制而縮減完整驗收範圍。


### A02 名稱報告內容指紋

- 後續補上 `files` 清單：相對路徑、實際讀取位元組數與 SHA-256，包含原始
  opaque 負載；ZIP 另記整包 `archiveSha256`。原始二進位仍不作名稱內容掃描，
  不修改負載；雜湊不代表上游來源、簽章或通知已核准。
- TDD：新增解包／ZIP 文字與原始負載的独立 hashlib 比對及負載變更斷言，
  先因缺 `files` 欄位 RED，最小實作後 12 項名稱掃描測試 GREEN。
- 這補充前節沒有內容雜湊的限制，不改變其他驗收缺口。失敗報告可為部分清單，
  不可作完整交付 manifest；停止寫入仍是前置條件，沒有原子快照／TOCTOU 保證。
- 本輪完整 Python 回歸 43 項：42 通過、1 Windows 專用跳過；`git diff --check`
  通過。未執行新的 Windows CI，也未把本機測試當作正式交付包驗收。

### A02 ZIP／解包內容配對門檻

- CLI 新增 `--archive --unpacked <directory>`，獨立掃描兩側並比對普通檔案
  相對路徑、大小及 SHA-256。總體成功必須兩側名稱通過且內容相同，不能拿
  各自綠燈但負載不同的候選當作同一包驗收。
- TDD 先以真實 CLI 重現缺少參數 RED，再最小實作 GREEN；另核對原始負載
  改變、解包殘留及兩側相同但名稱衝突的必要通知都會令總體失敗。
  13 項名稱掃描測試通過，工具不修改候選或代為解包。
- 比對範圍是普通檔案，不是空目錄、權限／時間／NTFS metadata 等價；名稱
  掃描仍檢查兩側目錄。此為驗收工具門檻，正式候選及 Windows 實驗仍未完成。
- 完整 Python 回歸 44 項：43 通過、1 Windows 專用跳過；diff 格式檢查通過。
  本輪沒有執行新的 Windows CI，未增加任何正式交付放行宣稱。

### A02 Windows 11 x64 獨立企業候選組裝器

- 2026-09-26 按 TDD 先新增 `tests/test_enterprise_package.py`；因組裝器不存在，5 個公開行為案例 RED。最小實作 `scripts/ccode/build_enterprise_package.py` 及中性 `docs/ccode-enterprise-usage.md` 後全部 GREEN。
- 組裝器拒絕既有輸出路徑，從全新 staging 只複製 `ccode.exe`、usage、`manifest.json` 與顯式必要通知；不從現有 AI Tools bundle 或任何目錄遞迴複製，因此不會把 data/profile/runtime/session/temp、其他工具或更新殘留帶入候選。
- 輸入 provenance 必須是 schema 1、Windows/x64、完整 40/64 hex adapter revision、引擎／官方 manifest hash 和 HTTPS 來源；每份通知必須是 regular file 並匹配顯式 SHA-256。通知原 bytes 同時在解包鏡像與 ZIP 驗證，缺少、hash 不符、link、Windows 危險檔名或大小寫重名均 fail closed。
- ZIP 使用固定 entry timestamp／mode／順序；候選 `manifest.json` 記錄 Windows 11 build 22000+、wrapper hash、engine/adapter provenance、檔案與通知 hash、公開文字／opaque 二進位邊界、排除的動態資料及 `external-gate-not-asserted`。完成後直接重用 `package_audit.py` 比對 archive／unpacked path、size、SHA-256 及名稱報告；衝突時不發布輸出。
- 本機完整 Python 回歸 59 項通過、1 項 Windows API 專用跳過；六組 C++ 回歸通過。這只證明固定本機工具鏈下的 deterministic 組裝行為，尚未在 Windows 11 x64 以真實 `ccode.exe`、核准禁用名稱及法律／合規方提供的通知執行。
- 下一個 TDD 切片先把 release contract 改為禁止任何 workflow 同時指向 `Auto Release AI Tools Portable` 並以 `gh release upload` 發布 `ccode.exe`，確認舊檔存在時 RED；再移除 `.github/workflows/append-ccode-release.yml` 至 GREEN。這只停止已知錯誤發布路徑；在再分發／通知、核准名稱及簽署門檻完成前，刻意不建立自動企業 release。
- 組裝器提交 `0a65e222d2805ae9c75dd70cdec17814e845e424` 已推送。補充 Windows Server workflow run `36207722704` 的兩個 test、兩個 workspace-boundary 及 cross-version 共五個 jobs 全部為零 steps；每個 check annotation 均指出帳戶近期付款失敗或 spending limit 需提高。這不是產品測試失敗，亦沒有執行組裝器或產生 Windows 11 x64 證據。
- 仍須取得再分發／通知核准、建立受保護的獨立發布流程並保存實際 Windows 11 x64 archive hash 與掃描報告；A02 仍未通過，A20 簽署門檻亦未因此完成。

### A03／A20 內嵌原始負載篡改：新增獨立 Windows 驗收門檻

- 新增 `tests/ccode/payload-integrity.py`：以 Windows data-only resource loading
  讀取原始負載及 metadata，先獨立計算 SHA-256 確認一致，再於臨時封裝副本的
  唯一完整負載中改變一個位元組。原始建置、前端其他位元組及 metadata 不修改。
- 驗收要求健康副本 self-test 成功；受損副本 self-test 與一般 `--print` 啟動
  均退出 64、stdout 空、stderr 只有 E_CHECKSUM；整個臨時根目錄不得產生
  engine.exe 或 JSONL。一般啟動僅提供假 token 及 loopback URL。
- TDD：先因 fixture 不存在 RED，再驗證只改一個指定負載位元組、拒絕找不到／
  多份負載及不符 checksum 的 fixture。workflow guard 先因缺少步驟 RED，再加入
  兩版本獨立必跑門檻；不允許 continue-on-error，不讓其他驗收失敗遮蔽本案例。
- 本機完整 Python 46 項：45 通過、1 Windows 專用跳過。新增 Windows resource
  讀取及實際封裝篡改啟動案例尚未執行；本機 fixture GREEN 不是 A03／A20 通過。
  此案例也不替代簽章失敗、更新中斷、可信來源／隨包來源清單及回退全矩陣。
- 本輪重新查閱 Actions run list 與 `108261875239` annotation：最新可見 run
  仍為 `36192868765` 的未啟動帳戶限制失敗，沒有新的 Windows 結果；歷史
  annotation 不能證明帳戶現在已修復或仍未修復，沒有盲目重跑或更改計費。

### A03 原始負載解出／快取修復：擴充待跑 Windows 門檻

- 同一完整性驗收在篡改拒絕之後恢復健康封裝副本，使用本機 401 API fixture
  要求實際引擎到達 `/v1/messages`，並核對 runtime/engine.exe SHA-256 與
  每個位元組均等於內嵌原始負載。之後更改快取負載末尾一個位元組，要求下一次
  啟動先恢復正確負載，再以真實引擎到達 fixture；不允許殘留 engine.new。
- 每次要求只有一個模型請求、E_GATEWAY_AUTH 分類及假 token／提示不回顯；
  不重試失敗驗收。原始來源建置保持不變，操作僅限本案例臨時副本與獨立資料。
- 新增 401 fixture 的本機 TDD：缺介面 RED，再用真實 loopback HTTP 驗證
  401、請求路徑及不保留／回顯請求內容 GREEN。完整 Python 47 項：46 通過、
  1 Windows 專用跳過。這不是實際引擎解出／快取修復已通過的證據。
- Windows 案例待 runner 可用後執行。既有 PrepareRuntime 邏輯未因測試而改動；
  也不把對內嵌 metadata 的一致性當成簽章信任、上游來源或再分發批准。

### Runtime 解出路徑：拒絕連結及異常檔案類型

- 檢查 PrepareRuntime 發現其原先直接開啟 engine.new，沒有先排除 symlink；
  此為程式碼確認的跟隨連結風險，不宣稱已在 Windows 重現實際外部覆寫。
- TDD 先新增原生路徑檢查測試並因缺 header RED，再實作 ValidateRuntimePaths
  至 GREEN；以真正符號連結驗證 engine.new 被拒絕且外部 sentinel 保留。
  另驗證 engine.exe 是目錄及非法 hash 拒絕。前端在建立 runtime、開啟 lock
  或寫出候選之前調用檢查，錯誤為 E_RUNTIME_PATH。
- runtime 根／hash 目錄及 prepare.lock、engine.exe、engine.new 均排除連結；
  Windows 另排除 reparse point。未存在的項目正常允許，已有普通快取仍走
  原有逐位元組核對及修復流程。不跟隨連結寫入不是同身份並發攻擊隔離，
  檢查與開啟之間仍有 TOCTOU 邊界，不宣稱完整 OS 安全沙箱。
- 新增 Windows 實際 CLI junction 案例：在臨時副本將 runtime 指向另一目錄，
  要求 E_RUNTIME_PATH、零額外檔案及 sentinel 不變，再移除 junction 本身，
  繼續健康解出及損壞快取修復驗收。該 Windows 案例尚未跑，不算通過。
- 本機六組 C++ 全部通過；Python 47 項中 46 通過、1 Windows 專用跳過；
  diff 檢查通過。沒有降低任何原有長 cwd、TLS、console 或正式交付門檻。

### Runtime 解出候選硬連結保護

- 延續連結檢查，以真正 hard link 將 engine.new 與外部 sentinel 連在一起；
  原 ValidateRuntimePaths 將其當普通檔案接受，測試在 hardLinkRejected 斷言
  實際 RED。新增普通 runtime 檔案 link count 必須為 1，查詢失敗亦拒絕，
  測試 GREEN，外部內容保留。不改動正常快取核對／修復路徑。
- 新增 Windows 真實 CLI hard-link 案例：建立候選硬連結，要求退出 64、只有
  E_RUNTIME_PATH、sentinel 位元組不變且未解出 engine.exe；清理只移除本案例
  建立的硬連結，再接續正常引擎驗收。此 Windows 案例尚未執行。
- 本機六組 C++ 全部通過；Python 47 項：46 通過、1 Windows 專用跳過；diff
  檢查通過。檢查／開啟間的 TOCTOU 限制仍保留，不能宣稱同身份攻擊隔離。

### A13 可重建索引不得透過硬連結覆寫原始歷史

- 在本機真實檔案系統將 session-index/<uuid>.json.new 硬連結到有效原始 JSONL，
  呼叫 ListWorkspaceSessions 重建索引；舊實作在「原始 history bytes 不變」
  斷言實際 RED，證明快取截斷寫入會破壞權威會話檔，不只是推測風險。
- 最小修補：已存在的索引候選必須是普通且只有一個硬連結的檔案；查詢出錯或
  不符條件以 E_SESSION_INDEX_WRITE 拒絕。原歷史、已提交索引及連結候選均保留；
  僅移除本測試建立的候選硬連結後，正常重建與列表恢復。原始 JSONL 不重写。
- 原生測試 RED→GREEN，另新增 Windows 實際 --sessions 案例，要求只有中性
  錯誤、stdout 空、三份既有內容逐位元組不變及解除候選連結後成功列出歷史。
  Windows CLI 新案例尚未跑，未宣稱已通過兩版本引擎或目標端點驗收。
- 六組 C++ 本機回歸通過；Python 47 項中 46 通過、1 Windows 專用跳過；
  portable fixture 語法及 diff 檢查通過。此修補不解決並發替換／TOCTOU，
  也不將 A13 損壞恢復、A16 會話並發等完整門檻視為完成。

### 候選驗證收據：保留探測期間出現的 pending 證據

- 以獨立 staged candidate 呼叫 ValidateProfileCandidate，在 probe callback 中
  寫入 validation.json.pending 模擬探測期間出現的中斷證據。舊實作以截斷
  stream 覆寫並移走該檔，在「原 pending bytes 保留」斷言實際 RED。
- 改為獨占建立：Windows CREATE_NEW，POSIX O_CREAT|O_EXCL；寫入後刷新、
  關閉才沿用原提交步驟。既有 pending 不覆寫，以 E_CANDIDATE_WRITE 拒絕；
  自身寫入失敗的部分檔亦保留。測試要求原證據不變且沒有 validation.json。
- 使用隔離候選避免把失敗後已建立的工作區 registry 當成乾淨候選重試；未
  刪除產品所建立的驗證快照。最終測試重新在舊寫法確認 RED，修補後 GREEN。
- 六組 C++ 回歸通過；Python 47 項中 46 通過、1 Windows 專用跳過；diff
  檢查通過。Windows 分支未執行，不宣稱已驗證 Windows 崩潰持久性。
- 此變更只處理候選收據配置時的覆寫；rollback-validation 的寫入仍需獨立
  TDD 驗證。rename 前並發替換、完整斷電／磁碟滿恢復矩陣仍非已完成項目。

### 回退驗證收據：保留探測期間出現的 pending 證據

- 使用另一個正式 PrepareProfileRollback 建立的候選，在第一個歷史 probe
  寫入 rollback-validation.json.pending。舊程式在兩個 probe 完成後截斷、
  移走該檔，於原 bytes 保留斷言實際 RED；不是只測 helper 是否存在。
- 回退收據改用與候選收據相同的獨占建立原語，碰到既有 pending 回報
  E_ROLLBACK_WRITE。原證據完整保留、不產生 rollback-validation.json；
  已完成的內層 validation.json 保留，不自動刪證據或重試。
- 另驗證只有內層收據不能啟用回退：ActivateProfileRollback 拒絕，整個
  候選檔案集合／內容、active pointer、目前 profile 與 preservation snapshot
  均保持不變。另一個乾淨候選的原完整驗證／啟用回歸仍通過。
- 六組 C++ 本機回歸通過；Python 47 項中 46 通過、1 Windows 專用跳過；
  diff 檢查通過。這不替代 Windows 實際引擎、磁碟滿、斷電或並發替換驗收。
- 再查 Actions：最新可見仍是 36192868765、SHA 454abc2 的 terminal failure，
  沒有新 Windows 結果。本輪未推送／重跑，不以歷史付款錯誤推斷當前帳戶狀態。

### 驗證收據發布：不得替換探測期間出現的正式證據

- 延續 pending 保全，分別對候選及回退的 public validation API 建立獨立
  候選，於 probe 中建立正式 validation.json／rollback-validation.json。
  本機舊 rename 寫法均在「原正式證據 bytes 不變」斷言實際 RED。
- 新增不替換目的檔的發布原語：Windows MoveFileExW 不指定 REPLACE_EXISTING；
  POSIX 以同目錄 link 原子建立目的項目，成功後才 unlink pending。不支援或
  衝突均拒絕，不退回可替換 rename。分別回報 E_CANDIDATE_WRITE／E_ROLLBACK_WRITE。
- 兩輪 RED→GREEN 驗證既有正式檔不變、新 pending 收據保留且可解析，回退
  active pointer 與現行 profile 不變；原健康候選及回退驗證回歸繼續通過。
- 六組 C++ 通過；Python 47 項：46 通過、1 Windows 專用跳過；diff 通過。
  RED 是本機 POSIX rename 行為的證據，不宣稱在 Windows 重現相同覆寫；
  Windows 新路徑尚未執行，仍需 runner 驗證中性錯誤及證據保留。
- 此原語不保證防止同身份惡意替換 pending，也未提供 directory fsync 的
  斷電持久性。POSIX 若發布後移除 pending 失敗，會保留兩項並報錯；不得
  因這一發布衝突案例通過而將完整更新／回退故障矩陣標成完成。

### A10／A11 串流 JSON：拒絕同一物件的重複欄位

- 原事件解析器採最後欄位值：同一 result 先 is_error=true 再 false 會被接受。
  新增 EventReader public Feed 測試，逐 byte 分片要求拒絕，舊實作在 rejected
  斷言實際 RED。這是前端事件解析的重現，不是實際上游引擎輸出此事件的證據。
- 在完整 JSON 行的解析 callback 逐物件追蹤已解碼 key，遇到重複欄位回報
  E_PROTOCOL_DUPLICATE_KEY，於 Event 改動 turn 狀態之前停止。錯誤不包含 key
  或原始內容，後續 Feed 不能恢復該 reader；不挑選第一或最後欄位值。
- 測試覆蓋 result、巢狀文字、Unicode escape 同名 key、重複 session_id 的
  每個 byte 分片點，要求 session／complete 不變。不同兄弟物件及後續事件
  重用相同 key 仍正常；既有 UTF-8、工具註冊與事件順序回歸不變。
- 六組 C++ 本機回歸通過。前端另以 AddressSanitizer／UndefinedBehaviorSanitizer
  執行通過。Python 原執行 handle 中斷後已不存在，重新執行取得 47 項中
  46 通過、1 Windows 專用跳過；diff 檢查通過。
- Windows 原生測試及真實引擎回歸尚未執行。此檢查只拒絕前端收到的歧義
  事件，不代表能撤銷引擎已執行的工具副作用，也不等於企業 gateway 全面驗收。


### A16 細粒度會話並發（Windows 11 x64 待驗）

- 2026-09-26 將一般前端的 `active-profile.lock`／`frontend.lock` 改為 shared，
  snapshot、stage、validate、activate、rollback 及 pending archive 保持 exclusive；
  profile registry、legacy restore 與 session index/list 由短期 `metadata.lock` 序列化。
- 每次真實 engine turn 在啟動子進程前取得獨立 session exclusive lock。既有 session
  使用 `--resume <uuid>`；新 session 先由前端固定 UUID，再使用 `--session-id <uuid>`，
  EventReader 只接受相同身份。同 session 第二 writer 回報 `E_SESSION_BUSY`／exit 75，
  不啟動引擎；不同 session 不共用 writer lock。
- session lock namespace 以實際 profile 父目錄為錨，令由主 data root 選取 candidate
  與直接以 candidate root 啟動時得到同一鎖。profile、鎖目錄及既有鎖檔拒絕
  symlink／Windows reparse point；既有鎖檔還要求 regular file 且 hard-link count 為 1。
  這是程序協調，不宣稱成為同身份惡意攻擊或 TOCTOU 的 OS 安全邊界。
- TDD RED→GREEN 已覆蓋：缺少 concurrency API、新 session 固定身份、candidate 兩種
  data root 的相同鎖路徑、lock root／中間目錄／profile symlink、hard-linked lock、
  metadata lock 不進 snapshot/candidate manifest，以及 workflow 缺少獨立 required gate。
- 新增 `tests/ccode/concurrency-integration.py`：不用 sleep 猜測，以 `threading.Event`
  阻塞 fixture 回覆；要求同 session 第二程序 exit 75、stderr 僅 `E_SESSION_BUSY`、
  零第二模型請求及零 transcript 寫入；另要求兩個不同 session 在任一回覆釋放前
  都已到達 `/v1/messages`，完成後各自 transcript 身份及 marker 不互相污染。
- 本機六組 C++ 通過；Python 48 項通過、1 項 Windows API 專用跳過；workflow guard、
  Python syntax 與 `git diff --check` 通過。MinGW 非權威交叉編譯仍停在既有 bcrypt
  header 差異，不能代替 MSVC build。Windows 11 x64 真實引擎案例尚未執行，A16
  仍不得標為完成。
- 遠端 SHA `3605dc1` 的 GitHub run `36202849568` 於 2026-09-25 因帳戶付款／spending
  limit 未啟動任何 step；這不是產品測試失敗，也不是 Windows 11 驗收證據。

### Windows 11 x64 普通帳戶專用驗收入口（待實機執行）

- 2026-09-26 新增手動觸發的 `.github/workflows/test-ccode-windows11-x64.yml`，只選取
  帶 `self-hosted`、`Windows`、`X64`、`windows-11` 標籤的 runner，避免把 GitHub
  託管的 Windows Server 結果誤記為 Windows 11 x64 放行證據。
- runner 先由 `scripts/ccode/assert-windows11-x64.ps1` 驗證 Windows 11 Client、build
  不低於 22000、OS 與進程均為 x64，並同時拒絕已提升的 Administrator token 及屬於
  本機 Administrators 群組的帳戶。證據 JSON 不記錄使用者名稱或憑證，只包含平台、
  權限布林值、執行檔 SHA-256、引擎版本、版本輸出與 adapter revision。
- 兩個固定引擎版本 `2.1.221`、`2.1.282` 均須通過 Python 回歸、Windows 原生測試、
  portable／真實工具／workspace alias／workspace boundary／payload integrity／gateway／
  session concurrency，之後才執行舊→新→舊 cross-version 恢復。所有 gate 都是 required，
  無 `continue-on-error`。
- workflow 與驗證腳本存在只代表驗收入口已建立，不代表 Windows 11 x64 已通過。
  尚需在符合標籤且以普通非管理員帳戶執行的真實 Windows 11 x64 runner 上取得成功 run；
  在此之前 A01、A08、A12、A14、A16 等相關平台門檻仍保持待驗。
- A16 推送後的 SHA `ba23032bd2e6932630ae6c62b05ffd4715a850b1` 對應 GitHub run
  `36204213325`（2026-09-26T00:16:17Z）。兩個版本、cross-version 及 long-workspace
  共五個 jobs 都是零 steps；check-run annotation 明確指出帳戶近期付款失敗或需要提高
  spending limit。這不是產品測試失敗，也不是 Windows 11 x64 驗收證據。


### A01／A05 Windows 11 x64 獨立離線包試運行（待實機執行）

- 2026-09-26 依 TDD 先在 `tests/test_ccode_workflow.py` 加入缺少 verifier 時會失敗的 public contract，確認它必須驗證 Windows 11 x64 普通帳戶、非 loopback route／adapter、服務／驅動 inventory、program／data manifest、真實 tools fixture 及 JSON evidence；再最小實作至 contract test 通過。
- 新增 `scripts/ccode/accept-offline-windows11-x64.ps1`，刻意不接入 connected GitHub workflow。它重用 `assert-windows11-x64.ps1`，只接受 Windows 11 Client build 22000+、OS／process x64、未提升且不屬 Administrators 的帳戶；任何非 loopback default route 或仍為 `Up` 的非 loopback adapter 都回 `E_OFFLINE_ROUTE`。
- 驗收在中文／空格 program、workspace、data 路徑複製候選，執行 `--version`、`--ccode-self-test`、`--package-manifest`、`--workspace-id`，再以新增的 `tools-integration.py --acceptance-root` 模式在同一目錄驅動真正引擎及 Write／Edit／Read／Grep／Glob／Bash。API 僅為 loopback deterministic response fixture；證據固定標示「not a live model or enterprise gateway」。
- 執行前後保存 program manifest、data root／workspace manifest、service 與 `Win32_SystemDriver` inventory SHA256 及 added／removed／changed 集合。新增 service／driver 失敗；program 只允許原 `ccode.exe` 及隨包 manifest hash 對應的 `runtime/<sha256>/engine.exe`、`prepare.lock`，其餘動態資料或內容變更失敗。
- route／adapter 自訂名稱只寫 SHA256；command evidence 會把 acceptance root、repository root、來源執行檔及 user profile 替換為固定 placeholder。證據不寫真實 token 或完整提示。PowerShell 7／Python 3 明列為驗收 harness 依賴，不列作交付 runtime 依賴。
- 本機已記錄 contract RED→GREEN；六組 C++ 回歸通過，Python 54 項通過、1 項 Windows API 專用跳過，另有 Python syntax、workflow guard 及 `git diff --check` 通過。macOS 沒有 `pwsh`，故本輪無 PowerShell AST 或 Windows cmdlet／實際執行證據。尚須在真正斷開全部非 loopback 網路的 Windows 11 x64 普通帳戶上執行並帶回 JSON；腳本存在不代表 A01／A05 通過，更新／搬移／重新打包矩陣也仍未完成。
- 實作提交 `08e94b17ec0b9ae5621c14252e62bd8256595332` 已於 2026-09-26 推送；補充 Windows Server workflow run `36207073887` 的兩個 test、兩個 workspace-boundary 及 cross-version 共五個 jobs 全部為零 steps。check-run annotation 明確指出帳戶近期付款失敗或 spending limit 需提高。這不是產品測試失敗，也沒有產生 Windows 11 x64、離線或 PowerShell verifier 執行證據。


### A08 Windows 11 x64 工作區 current-directory 邊界（待實機執行）

- 2026-09-26 依 Microsoft `SetCurrentDirectoryW`／`CreateProcessW` 的 current-directory
  限制，停止把 extended-path cwd 的 `WinError 267` 當成可由 `longPathAware` 修復的
  工具問題。本機 A 方案明確不支援 process cwd 超過該 Win32 邊界；長檔案、profile
  或 snapshot 可用不代表引擎 cwd 可超長。
- TDD 逐案例 RED→GREEN 新增 `workspace-boundary.hpp`：258 字元接受，259 以上回
  `E_WORKSPACE_PATH_TOO_LONG`；一般 UNC 及 `\\?\UNC\` 回
  `E_WORKSPACE_UNSUPPORTED`；`\\?\`／`\\.\` device namespace 回
  `E_WORKSPACE_PATH`。比較 namespace prefix 時大小寫不敏感。
- 前端新增 `--workspace PATH`。預設仍使用啟動 cwd；顯式相對路徑以啟動 cwd
  解析，必須是存在的本機目錄。工作區 UUID、session list／resume、候選單一
  probe 及真正 engine `lpCurrentDirectory` 都使用同一選定路徑；相對 `--data-dir`
  的基準不變。
- `tools-integration.py --workspace-boundary-only` 在 Windows 建立中文／空格短工作區，
  比對隱式 cwd 與顯式 `--workspace` UUID；再由短 cwd 傳入實際已建立的 259+
  本機路徑、UNC 與 device namespace，要求 exit 64、stdout 空、固定中性 stderr、
  不建立被拒 data root 且零 API request。舊 `--long-workspace-only` 與會在產品啟動前
  失敗的 extended cwd 已移除。
- 本機六組 C++ 回歸通過；Python 50 項通過、1 項 Windows API 專用跳過，另有
  `py_compile`、workflow guard 及 diff 檢查通過。這些只驗證 helper／門檻接線，
  不能替代 MSVC 建置或 Windows 11 x64 執行。`.github/workflows/test-ccode-windows11-x64.yml`
  已改用新 gate；在符合標籤的普通帳戶 runner 取得兩版本成功證據前，A08 仍是
  待驗、整體仍未放行。


### A03 隨包來源清單與解出負載證據（待 Windows 11 x64 實跑）

- 2026-09-26 依 TDD 先加入會失敗的 build provenance、公開 manifest 命令及 Windows 11
  evidence workflow contract，再最小實作到本機 contract tests 通過。
- `scripts/ccode/build.ps1` 現要求顯式完整 `AdapterRevision`，拒絕虛構預設值；下載官方
  `manifest.json` 與 `win32-x64/claude.exe` 後，先以官方 checksum 驗 payload，再內嵌
  schema 1 metadata：package／platform／architecture、adapter commit、engine version、
  SHA256、size、官方 manifest URL／SHA256 與官方 payload URL。三個 build workflow 均
  傳入當次 `${{ github.sha }}`。
- `ccode --package-manifest` 在 profile parsing／目錄建立前執行；只在 metadata schema、
  Windows x64 目標、完整小寫 commit／SHA256、官方 URL、resource size 與 resource 101
  digest 全部一致時輸出單行 JSON。失敗只回中性 `E_PACKAGE_METADATA` 或 `E_CHECKSUM`。
  `--ccode-self-test` 共用同一完整驗證，不再只核對單一 digest 欄位。
- `tests/ccode/package-manifest-integration.py` 以 `LoadLibraryExW(...AS_DATAFILE)` 讀取
  resource 101／102，不執行候選 entry point；把負載實際寫到暫存檔後重新計算 SHA256
  及 size，比對公開命令與 embedded JSON，確認 manifest／self-test 不建立 profile 或
  runtime，並輸出 `windows11-evidence/<version>/package-provenance.json`。證據包含 ccode
  executable、embedded metadata 與 extracted engine hashes，不包含使用者名稱或憑證。
- 現有 `payload-integrity.py` 已改用 schema 1 的 `engineSha256`，仍要求篡改 embedded
  payload 時 self-test 與正常 startup fail closed，且真正 runtime extraction／cache repair
  與原始 resource 逐位元組相同。
- 目前只完成程式、TDD contract、Python syntax 與本機回歸入口；尚未在符合標籤的
  Windows 11 x64 普通帳戶 runner 執行兩個固定版本。因此 A03 仍未通過，整體仍未放行。
  來源清單本身也未簽名，不能當作 A20 的可信簽署者或供應鏈真實性證明。
- A03 實作提交 `3140cc0fd5f864dd93494fd5b5cf51c42e142d59` 已推送；對應補充
  Windows Server workflow run `36206290494`（2026-09-26T00:50:08Z）的兩個 test、
  兩個 workspace-boundary 及 cross-version 共五個 jobs 均為零 steps。check-run annotation
  明確指出帳戶近期付款失敗或 spending limit 需提高，因此不是產品測試失敗，也沒有產生
  package provenance 或 Windows 11 x64 驗收證據。
- 查詢 repository Actions runners 結果仍為 `total_count: 0`。Windows 11 x64 專用 workflow
  需要 `[self-hosted, Windows, X64, windows-11]` runner，故本次未手動排入一個必然等待的
  run；須先提供符合要求且以普通非管理員帳戶執行的 runner。

### A04 公開／runtime／binary metadata 邊界清單

- 2026-09-26 依 TDD 先擴充 `environment-tests.cpp`、release contract 與企業包測試：因 `boundary.hpp`、`--boundary-manifest` 及 builder `--boundary` 尚不存在而 RED；再作最小實作至 GREEN。
- `ccode.exe --boundary-manifest` 是固定 schema、零秘密值的查詢入口，位於 options parse、data 目錄及 runtime 解出之前。它列出公開 exact／prefix、inherited filtering、`A_`／`C_` alias expansion、profile-relative HOME／TEMP、強制 retry／traffic 值，並明示 `originalRuntimeNamesPresent=true`、`processTreeNameFree=false`。
- binary metadata 明列 PE resource 101 為 opaque embedded engine、102 為 validated package provenance JSON；兩者均未宣稱名稱內容掃描，publisher signature 為 `not-asserted`。必要通知來源是 enterprise package manifest，launcher 不改寫通知。
- `build_enterprise_package.py` 現要求實際 `--boundary-manifest` JSON；只有完整符合 Windows/x64、minimum build 22000 與上述固定欄位才接受，否則 `E_BOUNDARY` 且不建立輸出。通過後把同一 validated document 保存為 `manifest.json.runtimeBoundary`。
- `package_audit.py` 仍掃描 manifest 公開文字。因此若核准受限名稱禁止 boundary 中必須如實記錄的原始 runtime 名稱，候選會 fail closed；不能刪除／改寫清單規避。此時是 A02／A04 政策衝突，須由需求／合規決策解決。
- 本地測試只能證明 static contract 與組裝器拒絕行為；尚未在 Windows 11 x64 普通帳戶以真實封裝確認零副作用，也沒有核准名稱／通知政策，因此 A04 仍為「部分」，不是放行。
- 本輪完整本地回歸為 Python 61 項（60 通過、1 項 Windows process/thread API 專用跳過）、六組 C++ 全通過；另有 `py_compile` 與 `git diff --check` 通過。loopback 測試需在允許本機 bind 的執行環境完成。這些仍不是 Windows 11 x64 實機證據。

### A05 完整企業候選資料分離／搬移／重新打包驗收器（待 Windows 11 x64 實跑）

- 2026-09-26 依 TDD 先新增 `tests/test_enterprise_lifecycle.py` 與 workflow contract；因
  `enterprise_lifecycle.py`、完整候選驗收 PowerShell 及搬移後 resume fixture 不存在而
  RED，再作最小實作至 GREEN。
- `scripts/ccode/enterprise_lifecycle.py inspect` 不信任既有 `package-audit.json` 的
  passed 字樣：只接受 `unpacked/`、audit 及唯一 ZIP，從 audit scope 取核准受限名稱後
  重新掃描 archive／directory，重算 archive SHA256 與逐檔 path/size/SHA256，並核對
  Windows/x64、minimum build 22000、manifest 白名單、notice hashes 及
  `excludedDynamicData`。保存 audit 未更新而解包內容被篡改時回 `E_LIFECYCLE_AUDIT`。
- `scripts/ccode/accept-enterprise-lifecycle-windows11-x64.ps1` 現以完整 enterprise candidate
  為輸入；複製整個 `unpacked/` 至中文／空格 program path，外置 workspace/data，先核對
  真實 `--package-manifest`／`--boundary-manifest`，再由既有 loopback fixture 驅動真實
  engine 與 Write/Edit/Read/Grep/Glob/Bash。執行後原 package files 必須不變，program root
  只允許 hash 相符的版本化 engine 與 prepare lock，data/profile/sessions/temp 不得滲入。
- 驗收器搬移完整 program directory，要求相同外置 workspace/data 的 UUID 不變；新增
  `tests/ccode/lifecycle-resume.py` 會列出既有會話並真實執行 `--continue`，檢查 loopback
  上游請求包含第一次六工具輪次的原提示。fixture 只替代模型回覆，仍不是 live model 或
  enterprise gateway。
- 搬移後以 executable 實際輸出的 provenance／boundary、原 usage／必要通知及 audit scope
  重新呼叫 `build_enterprise_package.py`。`enterprise_lifecycle.py compare` 強制 fresh
  repack 的 manifest 與解包 path/size/SHA256 和原候選完全一致，因此運行後 runtime、外置
  data、profile、session、temp 不能被帶入重新交付；兩個 archive SHA256 分別保存，且只
  記錄 byte-identical 狀態，不把跨 Python/zlib 壓縮 bytes 相同當唯一門檻。
- 本輪只有 macOS 上可執行的 helper／contract 測試，沒有 PowerShell 7，也未在 Windows 11
  x64 普通帳戶執行完整 candidate。故 A05 仍為待實機驗收；此案例亦不替代跨版本
  更新／回滾、簽署更新、正式 gateway 或真正斷網 A01。
- 本輪完整本地回歸為 Python 65 項（64 通過、1 項 Windows process/thread API 專用跳過）；
  六組 C++17 native/profile/environment/frontend/session/permission 測試全部通過，另有
  `py_compile` 與 `git diff --check` 通過。曾以 C++20 編譯會碰到既有 `u8string`／`char8_t`
  相容問題，依專案既有標準改回 C++17 後全綠；這不構成本輪產品測試失敗。


### A19 隱私安全失敗診斷（待 Windows 11 x64 實跑）

- 2026-10-01 依 TDD 先新增 `tests/ccode/diagnostic-tests.cpp` 與 workflow contract；因 `diagnostics.hpp` 及 build suite 尚不存在，純 C++ 編譯與 Python contract 都實際 RED，再作最小實作至 GREEN。後續另先加入選項值／`--` literal 的預掃描測試，舊 scanner 不存在而 RED，實作後轉綠。
- `ccode.exe --diagnostics PATH` 只在失敗時以 Windows `CreateFileW(..., CREATE_NEW, ...)` 寫入 JSON；既有檔、無法建立或 flush 失敗均只輸出 `E_DIAGNOSTIC_WRITE`，保留原 64／65／75／其他主要退出碼，不覆寫 sentinel。
- schema 1 僅記錄 `product=ccode`、Windows/x64、status、隨機 UUID v4 operation ID、中性 `E_*`、category、exit code，並把 arguments/environment values/prompt or content/credentials 四項 captured flag 固定為 false。`NeutralErrorCode` 不保存冒號後訊息；非核准格式降級為 `E_LOCAL`，因此 token、prompt、使用者路徑及 raw exception 不進報告。
- `scripts/ccode/test-windows.ps1` 以假 token、敏感 prompt 與私有 test root 觸發缺 gateway 設定，要求 exit 64、`E_GATEWAY`／network、可解析 operation ID、所有 privacy flag 為 false且三類敏感字串不存在；再以既有 sentinel 驗證不覆寫及主要 exit code 不被診斷寫入錯誤遮蔽。
- 本輪本地回歸為 Python 66 項（65 通過、1 項 Windows process/thread API 專用跳過）及七組 C++17 native/profile/environment/frontend/session/permission/diagnostic 全通過；MinGW 在補入其缺少的既有 MSVC `BCRYPT_SHA256_ALG_HANDLE` pseudo-handle define 後可編譯 `launcher.cpp`，只作語法補充證據。Windows 11 x64 實機尚未執行，GitHub Actions 最新 run 仍因 account payment／spending limit 在 0 steps 前被阻擋，因此 A19 仍為部分，不是放行。

## 2026-10-01：Actions 恢復執行與 Windows fixture 修正

- 舊 run `36209274280` 的 payment／spending-limit 阻擋屬歷史證據。
  run `36877451807`（`7e641402da2a092f6902250c1092f1277cab8ba1`）
  已實際執行兩引擎建置及測試，不能再把目前失敗歸因於額度。
  這只證明 runner 能執行，不宣稱已核實帳戶 billing 餘額或未来可用額度。
- `4e31750` 固定 workflow 測試 UTF-8 並保留 ZIP 原始惡意路徑。
  `36880031652` 已不見原 CP1252 解碼錯誤，但仍有 ZIP `3 != 4`；
  因此該切片不能視為 ZIP 安全驗收完成。
- `12aa402` 進一步修正 ZIP 讀取端：審核 `ZipInfo.orig_filename`，
  不使用 Windows 已正規化的 `filename`。新增模擬讀取端正規化的
  RED→GREEN 回歸，惡意反斜線必須拒絕、不得開啟內容；完整本機
  Python 70 項（69 通過、1 Windows API 跳過）。仍待該 SHA 的 Windows 結果。
- run `36880680854`（`83b11dbfdfcd59c1c41f78d24f037e58ee7d17fa`）
  已完成且整體失敗。兩引擎的建置、actual engine tools、workspace aliases、
  `Reject tampered embedded payload` 步驟成功；兩 workspace-boundary jobs 成功。
  這確認 `83b11db` endpoint 計數修正後該完整性步驟能通過，不代表 A20 全量通過。
- 同一 run 的 gateway 日誌：unreachable、401、429、截斷及語義 EOF 案例通過，
  但 TLS 未觀察到失敗 handshake；native Python 測試另有一個 TLS fixture
  失敗及 ZIP 路徑失敗。不得略過 TLS、關閉憑證驗證或以非零退出代替握手證據。
- 同一 run 的 concurrency：新 session identity 與同 session 第二寫入者
  提前拒絕通過；不同 session 同時到達 API 仍失敗。`25ca92a` 增加
  請求數、收尾後退出碼及 stdout／stderr 存在性證據，不打印原始內容，
  不放寬「任何回應釋放前兩個 session 都已到達」條件；本機 RED→GREEN，
  Python 71 項（70 通過、1 跳過）。Windows 結果尚未確認。
- 同一 run 的 cross-version 已通過候選精確拷貝、真實引擎候選續接及
  active／backup bytes 保全，後於來源變動拒絕的 candidate bytes 比較失敗。
  launcher 在來源驗證前建立 `candidate/profile/frontend.lock`；
  `4f9ed73` 對整個 candidate 使用精確的 `profile/frontend.lock`／
  `profile/metadata.lock` 排除，仍比較 candidate.json 及其他全部 bytes，
  其他位置同名檔不得排除。新增 RED→GREEN 回歸；Python 72 項
  （71 通過、1 跳過）。仍待 Windows 確認，不能宣稱跨版本全量成功。
- 上述證據均非乾淨 Windows 11 x64 普通帳戶實機證據；A01–A20 的外部
  gateway、端點政策、核准通知／簽署者、斷網及試運行門檻仍保留，未放行。

### Runtime 準備鎖並行修正（Windows GREEN 尚待確認）

- `36882992434` 的 2.1.282 job `110439192753` 真實並行案例 RED：
  requests=1、exit_codes=[64,0]、neutral_codes=[[E_RUNTIME_BUSY],[]]。
  檢查 launcher 確認 runtime/prepare.lock 原來以零等待 CreateFileW，
  所以不同 session 也會在共用 runtime 準備階段互相拒絕。
- 修正沿用 AcquireFileLock：prepare.lock 獨佔、有上限 30 秒等待，
  只對 sharing／lock violation 重試；非一般單連結檔案或 reparse
  仍以 E_RUNTIME_PATH 拒絕。取得鎖後仍獨立比較整個 embedded payload
  bytes，必要時修復，鎖只涵蓋準備階段，並不涵蓋模型請求。
  session.lock 仍零等待，不能將同 session 寫入衝突變成排隊／重放。
- 本機 Python 74 項（73 通過、1 跳過）、七組 C++17 suites、MinGW
  launcher 語法交叉編譯及 diff check 通過。MinGW 定義既有缺少的
  BCRYPT_SHA256_ALG_HANDLE 常數僅用於語法檢查，非 Windows runtime 證據。
- 此切片已有真實 Windows RED，Windows GREEN 必須以推送後的同一
  concurrency integration 驗證，尚未取得，不宣稱 A16 完成。
- 同一 job 的 TLS 診斷為 connections=2、transport errno=10054 兩次、
  HTTP requests=0、engine exit=1；只證明曾連接且未送 HTTP，不單憑 reset
  當作憑證拒絕或 E_GATEWAY_TLS 分類通過。

### 2026-10-01：準備鎖實際 CI GREEN 與 TLS 嚴格證據閘門

- run `36884421476`，SHA `ab3deb2c8ba7cef93294e17bc6691fa2d04482d6`，
  2.1.282 job `110443965688` 的 `Verify session writer concurrency` 成功：
  真實新 session identity、同 session 第二寫入者提前退出 75、不同 session
  在同一 workspace 並行且不覆蓋歷史均通過。這是準備鎖修正的 CI GREEN，
  不等於 Windows 11 普通帳戶 A16 全量實機驗收。
- 同 job 的 embedded payload tamper／cache repair 步驟通過；gateway 的
  unreachable、401、429、截斷及兩種語義 EOF 案例通過。TLS 仍失败：
  connections=2、transport errno=10054、HTTP=0、exit=1。
- native 步驟的唯一 Python failure 是 TLS fixture 的 server-side rejected
  event 未設置；客戶端憑證例外驗證仍保留。`210efa6` 本機實際 RED→GREEN：
  新增測試先因缺少 helper 失敗，再以握手失敗、有連線、零 HTTP、非零退出、
  明確 E_GATEWAY_TLS 的共同閘門通過。不接受單純 reset 或 E_ENGINE_API。
  engine integration 另要求 stderr 完整行 E_GATEWAY_TLS；不打印私人內容。
  完整 Python 75 項（74 通過、1 Windows API 跳過）。
- `210efa64d44979d81628c3afaefa470b2c8252c3` 對應 run `36885251680`
  已確認 in_progress。尚未取得實際 engine TLS GREEN，不能標記 TLS 通過。
- 查核時 `36884421476` 的 2.1.221 portable frontend 步驟成功，但 run
  仍 in_progress；跨版本及其餘版本不得由單一步驟成功推論完成。

### 2026-10-01：嚴格 TLS 實際 RED 與跨版本搬移末端 RED

- `210efa6` 的 run `36885251680` / job `110446797062`：native、portable、
  六工具、aliases、payload 與 concurrency 通過，但 gateway TLS 嚴格閘門仍失敗。
  server 記錄兩個 transport 10054、零 HTTP、exit 1；不能推論 TLS 分類成功。
- 原始碼檢查：frontend.hpp 的 assistant structured error 只特判
  authentication_failed／rate_limit；api_retry 一律 E_GATEWAY_RETRY；launcher
  未完成事件一律中性 E_ENGINE。當前沒有產生 E_GATEWAY_TLS 的實作路徑。
  因此新增 fixture 不只是觀測修正，還揭露 A18 分類實作缺口；不得將測試
  改成接受泛用 E_ENGINE 或憑空把任何 HTTPS 失敗歸類 TLS。
- `36884421476` cross-version job `110447779356` 已完成，升級／回退、
  候選所有會話驗證、pointer replacement failure、程式搬移均通過，最後
  外置 data root 搬移的 preserved profile bytes assertion 失敗。
  whole-data snapshot 包含各 profile 根鎖，切回 profile 比對卻排除根鎖。
  `6aec27a` 以同一精確根鎖政策擷取 baseline；巢狀同名檔仍比較全部 bytes。
  新測試實際缺 helper RED，再 GREEN；完整 Python 76 項（75 通過、1 跳過）。
  尚待修正後的 Windows cross-version GREEN，不能標記 A14/A15 完成。

### 2026-10-02：跨版本 CI GREEN 與 TLS 觀測競態修正

- 重新透過 GitHub API 確認 run `36886068270` 的 cross-version job
  `110453169843` success；兩個 workspace-boundary jobs 也 success。
  這是 CI 跨版本流程證據，不等於 A14/A15 全部故障矩陣或 Windows 11
  x64 普通帳戶實機驗收；整個 workflow 仍因 gateway 測試失敗。
- 最新 `d2c7719` run `36888569723` / job `110458089880` 日誌顯示 TLS
  connections=2、handshake_errors=[]、HTTP=0、exit=1、tls=false、engine=true。
  引擎退出與 server 記錄握手之間存在競態；驗收 predicate 現在可有界等待
  handshake event，實機 fixture 使用 3 秒。仍必須明確 E_GATEWAY_TLS，
  未觀測到事件、HTTP>0、exit=0 或泛用 E_ENGINE 均不能通過。
- TDD：新增延遲 event 行為測試，實際 RED（缺 handshake_timeout）；
  修正後完整 Python 82 項，81 通過、1 Windows API 跳過；diff-check 通過。
  Windows 整合 GREEN 尚待本次提交後的工作流，不宣稱 TLS 已通過。
- 同一最新 job 的 native probe 仍遇到 WinError 32/5 workspace 清理失敗；
  native process tree containment 尚未修復，不能忽略清理錯誤。
  native runtime 診斷測試另因 collector timeout 失敗，仍待定位。

### Native TLS probe process-tree containment（Windows GREEN 待取得）

- 對應實際 RED：`36888569723` / `110458089880` 探針父程序 exit=1
  後，workspace 刪除仍遇到 WinError 32/5。不可 ignore cleanup errors。
- 新增 `run_contained`：CREATE_SUSPENDED 啟動、加入 kill-on-close Job
  Object 後才 ResumeThread；使用臨時檔接收 stdout/stderr，避免子程序持有
  pipe 導致父程序退出後 communicate 卡住。正常退出／逾時都終止 Job，
  查詢 active processes=0 才返回；containment 失敗不允許無隔離執行。
- 新增真正 Windows 行為測試：父程序正常退出後仍持有 cwd 的子程序必須
  被清除；父程序逾時後也必須清除已寫入 ready marker 的子程序，並實際
  rmdir workspace。這兩項在 macOS 跳過，尚無 Windows GREEN，不能宣稱
  native cleanup 已驗收通過。
- 非 Windows 拒絕執行測試先 RED（缺 run_contained），再本機 GREEN。
  完整 Python 85 項：82 通過、3 Windows 專屬測試跳過；diff-check 通過。
  以上只是本機回歸證據，不替代 Windows Job Object 整合結果。

### Windows probe containment 實際結果與 readiness fixture 修正

- `0e6c1d7` run `36890483649`，jobs `110464560146`（2.1.282）與
  `110464560501`（2.1.221）：兩邊的 timeout process-tree 測試均 ok。
  兩個 native TLS probes 均輸出結構摘要並完成，未再出現 workspace
  WinError 32/5；這是原生探針清理的整合改善證據，不是 TLS 分類 GREEN。
- normal-parent-exit 測試兩邊實際 RED：parent returncode=1。
  fixture 用 print 的文字 newline，卻透過 binary pipe 要求 b'ready\n'；
  Windows CRLF 使 readiness assertion 失敗。改成固定五個 binary bytes
  b'ready'，parent 精確 read(5)，不放寬 marker 或 workspace rmdir 斷言。
  修正後的 Windows normal-exit GREEN 尚待新 CI。
- 兩邊嚴格 gateway 驗收仍 RED，tls=false/engine=true。2.1.221 即使有界
  等待後仍可能沒有 server handshake error；不能拿 transport reset 代替
  引擎 TLS 分類，亦不能宣稱 fault matrix 完成。
- 本機完整 Python 86 項（83 pass、3 Windows skip）、七個 C++ suites
  皆 exit 0；diff-check 通過。上述不替代 Windows 11 x64 普通帳戶實機。

### A19 gateway failure-report cause propagation（Windows integration 待驗證）

- 原實作 RunTurn 已輸出 E_GATEWAY_AUTH／E_GATEWAY_RATE_LIMIT，但 wmain
  對非零 Main return 一律寫 E_ENGINE，JSON 丟失已確認的中性分類。
- TDD：frontend 新增 failureCode 行為斷言先編譯 RED，再 GREEN；未知
  assistant error 保持 E_ENGINE，後續 success result 不清除原分類。
  RunTurn 將中性分類傳給 Main/wmain；協定錯誤及取消也保留其固定代碼。
  不傳 raw engine error/body、token、prompt，不改退出碼或重試策略。
- 401／429 actual gateway integration 現在使用事前不存在的 --diagnostics
  路徑，要求 report exact schema、UUID v4、network category、相同 exit
  code、四個明確 false 隱私欄位，以及相符的中性 gateway errorCode。
  不多發模型請求。純 verifier 缺 helper 實際 RED，再 GREEN；數字 0
  偽裝 privacy false 的案例也先實際 RED，再以嚴格 JSON 型別比對 GREEN。
- 本機 Python 87 項（84 pass、3 Windows skip）、七個 C++ suites pass。
  MinGW syntax check 的原生 SDK 缺 BCRYPT_SHA256_ALG_HANDLE；僅命令列
  以 nullptr placeholder 作 syntax check 後通過，不修改產品常數，也不
  宣稱其為加密／執行證據。MSVC build 與 401／429 JSON 實跑尚待 CI。
- 這只擴充 A19 的認證／限流故障覆蓋；TLS、DNS、expired certificate、
  其他故障矩陣及 Windows 11 x64 普通帳戶報告仍不得標記完成。

### Probe cleanup Windows GREEN 與下一步 TLS 取證

- `a21ecc0` run `36891462354`，jobs `110467849644`（2.1.282）及
  `110467850029`（2.1.221）：normal-parent-exit 與 timeout process-tree
  測試日誌均明確 ok；不再把 macOS skip 當成 Windows GREEN。
- 此 run 的 gateway TLS 仍失敗。原生 result 沒有已檢查的 execution-error
  subtype／errors array／structured TLS code，不能憑 binary string table
  含 SSL 字句就分類產品錯誤。追加固定欄位形狀旗標（success/error subtype、
  result/error 字串或 error object），不輸出其值。反射的 CERT_HAS_EXPIRED
  result 文字必須仍不能通過 structured TLS code 分類。
- 新 shape 行為測試實際 RED（缺欄位），再 GREEN；Python 88 項，85 pass、
  3 Windows skip；diff-check 通過。Windows 新旗標輸出待本提交 CI。
- GitHub runners API 查核 repository self-hosted runners total_count=0。
  Windows 11 x64 普通帳戶實機驗收環境仍未提供；hosted CI 與上述清理
  GREEN 不替代完整 A01–A20 實機／企業及离線證據。

### A19 interrupted-stream diagnostic coverage

- TDD: exact E_MISSING_RESULT / E_TRUNCATED_EVENT category assertions failed
  before the implementation and pass after mapping them to protocol. Unknown
  E_MISSING_RESULT_PRIVATE remains local. Canonical-renderer verifier test also
  failed before its helper existed, then passed; reflected substrings are refused.
- Three interrupted-stream integration cases now request a fresh diagnostic report
  and require exact schema, neutral cause/category, UUID v4, matching exit code
  and false privacy flags. Existing single-request/no-write checks remain intact.
- Local regression: 89 Python tests, 86 pass / 3 Windows skips; all seven C++
  suites exit 0; git diff --check passes. Windows execution of this slice pending.
- Run 36892763897 (bc95280) logs confirm 2.1.282 HTTP 401 and 429 checks pass,
  including the exact diagnostic verifier executed before the PASS marker.
  Overall run still fails; payload auth-fixture and TLS failures remain unresolved.
  This is hosted CI evidence, not Windows 11 ordinary-account acceptance.

### A19 unreachable endpoint diagnostic coverage

- Added a fixture-level test that actually failed because --diagnostics was
  missing; implementation now requests a fresh report and validates exact schema,
  canonical neutral cause/category and nonzero exit code. No extra engine request
  or retry is introduced. Unknown/private terminal text cannot choose a cause.
- Local Python regression: 90 tests, 87 pass / 3 Windows skip; focused fixture
  suite 11 pass. Mock-run PASS output is suppressed: this is not engine evidence.
  Windows execution of this change remains pending.
- Run 36893307372, completed 2.1.282 job 110473991313: native runtime, frontend,
  actual tools, aliases, payload integrity and concurrency steps pass. All three
  interrupted-stream cases pass before TLS (prior to the new A19 report checks).
- Native TLS result shape is success_subtype=true/result_text=true with
  is_error=true, no structured TLS code. Certificate fixture receives two
  transport resets (10054), HTTP zero; portable code still E_ENGINE. Therefore
  TLS remains RED; result text is not promoted to trusted TLS classification.

### A19 TLS report gate (not TLS classification GREEN)

- Fixture-level test first failed because TLS invocation lacked --diagnostics.
  It now requests a fresh report and, only after the existing strict certificate
  rejection/zero-HTTP gate, requires exact E_GATEWAY_TLS/network report schema
  with actual exit code. Existing no-write, privacy and TLS assertions remain.
- Mock unit test does not prove real engine TLS classification; its PASS output
  is suppressed. Local Python 91 tests: 88 pass, 3 Windows skip; diff-check pass.
  Actual TLS remains unresolved and must still fail if only E_ENGINE is emitted.

### Native probe cleanup regression and process-object wait

- Run 36894764599 / job 110478859618 (2.1.282) has actual RED:
  normal-parent-exit containment returns, but workspace.rmdir fails WinError 32.
  Earlier GREEN does not prove stable cleanup. Unreachable gateway with new
  exact A19 diagnostic checks passes; TLS remains RED.
- Added process-handle wait tests: missing helper produced actual RED, then
  GREEN. Before terminating the job, enumerate its members, retain synchronized
  process handles, verify membership against PID reuse, and wait for signaled
  process objects. Still require active count zero and parent wait; close all
  acquired handles. Enumeration, membership and wait failures fail closed.
- Final local Python: 93 tests, 90 pass / 3 Windows skip; diff-check pass.
  This is a candidate cleanup correction, not confirmed Windows GREEN: live
  process enumeration can race, and the unchanged workspace.rmdir assertion
  remains the actual Windows gate. No deletion retry or ignored cleanup error.

### Keep Python and native-runtime CI evidence independent

- The 1bf99b8 Windows Python cleanup regression caused test-windows.ps1 to
  be skipped inside a combined step. Both hosted and Windows 11 workflows now
  have separate required Python-contract and native-runtime steps. Native runs
  after Python failure only when build succeeded and the job is not cancelled;
  Windows 11 still additionally requires the ordinary-account platform gate.
- New workflow contract test produced actual RED for both missing independent
  steps, then GREEN. No continue-on-error or reduced assertions. Python failure
  still fails the job; native-runtime failure independently fails the job.
- Local regression: 94 Python tests, 91 pass / 3 Windows skip; diff-check pass.
  Actual execution of the split workflow and 012cb75 cleanup fix remains pending.

### A19 corrupted committed profile-pointer report

- Exact E_ACTIVE_PROFILE incorrectly categorized local; C++ assertion produced
  actual RED, then GREEN after exact data classification. Unknown suffixed code
  remains local; no broad ACTIVE prefix rule.
- Existing portable actual-product broken-pointer case now requests fresh
  --diagnostics and uses the shared exact JSON verifier for E_ACTIVE_PROFILE,
  data category, exit 64, UUID v4 and privacy flags. Existing original-byte and
  no-fallback-profile checks remain. No additional engine request.
- Local verifier rejects wrong category and extra private fields. Final Python
  95 tests: 92 pass / 3 Windows skip; all seven C++ suites pass, diff-check pass.
  Windows actual diagnostic report remains pending; not A19 overall GREEN.

### Payload 間歇失敗的隱私安全定位（2026-10-02）

- `caa505e` / run `36897079778` / job `110486643415`：普通 hosted Windows
  回歸中 active-profile 損壞案例及 exact data-category report 驗證通過；
  payload fixture 仍失敗於 `Engine did not reach auth fixture`，TLS 仍未分類。
  此證據不是 Windows 11 x64 普通帳戶放行。
- payload fixture 新增精確中性 renderer line 白名單摘要：僅 attempt、exit code、
  request count、auth/engine/retry 布林值，不保留原輸出、URL、token、prompt 或路徑。
  同時保留非零退出、恰好一次模型請求、hash/bytes 一致及無 disclosure 的要求；
  不重試引擎請求。新測試先因 helper 缺少而 RED，再 GREEN；96 項 Python
  回歸成功（3 項 Windows-only skip）。這是定位證據改善，不是 payload 問題已修復。

### A15 真實 snapshot launcher 強制中斷／重新備份案例

- 新增 `portable-integration.py` 案例：以約 58 MiB 合成 source file 保持真實
  hash/copy 階段可觀測，僅在 launcher 存活且 `.pending/profile` 已建立、manifest
  尚未出現時強制終止並等待退出。若觀察不到此階段或已發布 target，案例 fail，
  不以事後預置 pending 代替實際中斷。
- 中斷後 source bytes／active selection 保全；隨後新 snapshot 必須成功、UUID／
  manifest inventory／每檔 size/SHA256／copied bytes 全部相符，並保留原 pending
  evidence。無 engine/model request 重試；重新執行的是本地備份命令。
- helper 測試先因未實作而 RED，再 GREEN；97 項 Python 回歸成功，3 項
  Windows-only skip。尚待推送後 hosted Windows 與正式 Windows 11 x64 普通帳戶
  實跑。這不替代磁碟滿、斷電、activation／rollback 各中斷點或整個 A15 驗收。

### Payload cache repair 啟動失敗的 exact-code 定位

- `ee23db8` / run `36899884966` / job `110496106333` 實際日誌確認
  forced snapshot interruption／fresh backup hash case PASS（hosted Windows，
  不替代正式 Windows 11 x64）。payload attempt 1 失敗：exit 64、request count 0，
  auth/engine/retry 均 false；故此案例尚未抵達 auth fixture，不能推定為 gateway 問題。
- failure summary 增加 startup_code：僅 exit 64 且完整 terminal 去首尾 whitespace
  後精確等於 E_EXTRACT／E_RUNTIME_BUSY／E_RUNTIME_PATH／E_CHECKSUM 時記錄。
  未知碼、多行混合、附帶路徑及非 64 退出一律 null；不輸出 raw stderr。
  單元測試先 KeyError RED，再 GREEN；98 Python tests，95 pass／3 Windows skip；
  diff-check pass。這只是定位補強，不是 cache repair 已修復。
- 同一 job 的 TLS gate 仍因兩次 errno 10054 transport reset、零 HTTP request 而 RED；
  不將 reset 推定為憑證錯誤。正式 Win11 與完整 A15／A19 驗收仍未完成。

### A17 真實引擎 synthetic stdio MCP 驗收切片

- 新增固定 read-only `probe` 的 stdio MCP fixture；initialize／ping／tools/list／
  tools/call 使用與既有 permission MCP 相同的 newline JSON-RPC 模式，notifications
  不回應，未知工具或非固定參數拒絕。只保存固定合成 invocation marker。
- `tools-integration.py` 主流程新增獨立 mcp case：由實際 engine 接收 user
  --mcp-config，允許單一 fixture tool；要求初始 model request 真實列出工具、
  model 後續請求含成功 tool_result、server evidence 恰好一次 call、外置 data/session。
  不合併／繞過內部 permission MCP，不猜測多個 config 的效果；實際不相容則 fail。
- stdio subprocess test 先 missing file RED 再 GREEN；驗收 verifier 先 missing helper
  RED 再 GREEN，拒絕缺發現、缺結果、error result 及重複呼叫。100 Python tests：
  97 pass／3 Windows skip；diff-check pass。實際 Windows engine MCP case 待 CI，
  正式 Windows 11 x64、技能／子代理及核准第三方 MCP 尚未驗收；A17 不標通過。
- `c14eb7c` / run `36901139794`：兩版本 job 日誌均確認 payload tamper／actual
  extracted bytes／cache repair PASS；這是新一次 regression，不證明間歇問題已修復。
  TLS gate 仍 RED（transport errno 10054、零 HTTP），不改變故障分類／驗收門檻。

### A17 MCP 非互動拒絕／零 invocation 門檻

- 與 allowed MCP case 分開使用全新 app/data/workspace；同樣提供使用者
  --mcp-config，但不傳 --allowedTools，不能以成功 case 代替權限拒絕。
  要求真實 engine 列出 MCP tool、回傳 is_error=true tool_result，且 server
  invocation evidence 完全不存在；即使結果說拒絕，只要 server 執行過也 fail。
- verifier 新測試先因 denied 參數不存在而 RED，再 GREEN；成功結果偽裝拒絕及
  已執行但回錯誤均 fail。101 Python tests：98 pass／3 Windows skip；diff-check pass。
  Windows engine 實跑尚待驗證，不能推定 MCP 安全隔離或第三方批准。

### A17 workspace Skill 真實載入 gate

- 在獨立 workspace 建立合成 `.claude/skills/acceptance-probe/SKILL.md`，正文
  marker 不出現在 fixture 初始 prompt。模型 fixture 僅要求真實 `Skill` tool。
  要求初始請求列出 Skill、成功 tool_result、後續真實模型請求含正文 marker，
  且技能 source bytes 不變、session 位於外置 data，不以名稱可見替代載入成功。
- verifier 測試先 missing helper RED，再 GREEN；無後續請求、預先注入正文、
  缺結果、error result 均拒絕。102 Python tests：99 pass／3 Windows skip；
  diff-check pass。Windows Skill 實跑待驗證，A17 不標通過。
- 網頁工具未返回可讀官方文件，未取得可引用的網頁來源；此 fixture 的能力假設
  必須由指定版本實際執行驗證，不冒稱已經文件核實或第三方批准。
- run `36902300245` 的兩個 tools step 已呈 failure；完整 job logs 尚待可讀，
  不推定原因或以 static GREEN 覆蓋實際 failure。

### A17 MCP fixture params metadata 相容修正

- `f8709eb` / run `36902300245` / job `110504168347` 完整日誌：六工具 PASS，
  MCP discovery 與 model tool_result 已抵達 verifier，但 tool_result is_error。
  尚無實際呼叫 params／錯誤原因證據，不推定此修正必然解決 CI。
- fixture 原本整個 params dict 完全相等比較會拒絕 `_meta`，新增含合成
  progressToken 的實際 stdio subprocess test 先 KeyError(result) RED，修正後 GREEN。
  現只允許 name／arguments／可選 object `_meta`；name、完整 arguments 仍須精確
  等於固定 probe／固定 marker，metadata 不記錄、不影響權限或結果。
- 完整 Python 102 tests：99 pass／3 Windows skip；diff-check pass。
  actual-engine MCP 成功／拒絕及 Skill 仍待實跑，A17 不標通過。

### A17 擴展驗收與既有六工具／取消／權限回歸分離

- 新增 --mcp-only（allow／deny）及 --skill-only 專用入口，從 baseline tools main
  移出擴展案例，避免 MCP failure 阻止原本 8.3／cancel／crash／互動批准矩陣。
- hosted Windows 與正式 Windows 11 x64 workflow 都新增獨立必須通過的 MCP／Skill
  steps；使用 !cancelled() + build success，Win11 再要求 platform success，不依賴
  前一測試成功，不加 continue-on-error、不修改放行条件，Skill 不再被 MCP failure 隱藏。
- workflow contract 先四個 missing gate RED，再 GREEN。完整 Python 103 tests：
  100 pass／3 Windows skip；diff-check pass。新 gate 的實際執行尚待 CI，仍不是 A17 通過。

### 實跑 A17 證據與 TLS 協定觀測（2026-10-02）

- run `36903350745` / SHA `27cc6d8` / job `110507652789`（2.1.282）完整日誌
  已明確 PASS：MCP discovery／一次 invocation、未批准 MCP 零 invocation、Skill
  正文後續請求載入、六工具、8.3、取消／崩潰清理、互動權限、payload 修復與
  session writer 互斥。這是 hosted runner 的合成案例，不代替 Win11 普通帳戶、
  subagents 或已批准第三方矩陣，不將 A17 整體標通過。
- 同 job gateway TLS 仍 fail：兩次 transport errno 10054、HTTP 0，無可信 TLS
  cause。新增 fixture 的三 byte MSG_PEEK 粗分類，只輸出 tls-record／other／closed，
  不消耗或輸出原始 peer bytes、不改 engine 信任、代理或錯誤分類。短 prefix 的
  other 不能證明非 TLS；tls-record 亦不證明憑證拒絕。驗收門檻保持不變。
- 分類測試先 missing helper AttributeError RED；實作後完整 Python 104 tests：
  101 pass／3 Windows-only skip。實際 Windows 協定觀測仍待推送後結果。

### A17 真實 subagent 獨立子請求 gate

- 新增 --subagent-only 合成驗收：從真實第一個模型請求的工具 schema 選取
  Agent／Task（必須具 subagent_type、prompt、description），建立 workspace agent
  定義；父 prompt 不注入子正文，只有包含子 system context 的真實請求才返回
  固定 child result。父請求必須收到成功 tool_result 與 child marker，source bytes
  不變，session 位於外置 data，program 不得有 JSONL。
- verifier 先缺 helper RED，再 GREEN；無子請求、預載正文、只有 messages marker、
  缺結果、error result、缺 child marker 均 fail。兩個 workflow 加獨立 required gate，
  contract 先 missing gate RED，再 GREEN，不加 continue-on-error。
- 完整 Python 106 tests：103 pass／3 Windows-only skip；diff-check pass。
  Agent／Task 相容性與實際 child context 還須指定 engine 實跑驗證，不能以 verifier
  通過宣稱 subagents／A17 已通過；官方網頁工具未返回可讀內容，未冒稱文件核實。
- `6905c8f` / run `36904848916`：job `110512692716`（2.1.282）及
  `110512693091`（2.1.221）TLS 都觀察兩次 tls-record、HTTP 0、exit 1，仍無
  E_GATEWAY_TLS 或可信 structured cause。2.1.282 transport errno 10054；2.1.221
  assertion 時 handshake_errors 為空。TLS gate 維持 fail，不作憑證拒絕推定。

### A19 workspace／history 損壞的真實診斷報告 gate

- 在既有 portable engine 驗收的真實損壞 workspaces.json／四種 history metadata
  案例中加入 --diagnostics，每次 UUID 新路徑，要求 exit 64、stdout 空、stderr
  只有預期中性 code，再逐欄驗證固定 schema、UUID、data category 與 privacy=false。
  保留原本 registry／transcript bytes 不變斷言，不能以報告存在替代資料保全。
- helper 測試先 missing function RED；GREEN 後兩次呼叫必須產生不同報告路徑，
  缺報告、成功退出、私有 stdout 均拒絕。完整 Python 107 tests：104 pass／3
  Windows-only skip，diff-check pass。新實際 Windows report gate 尚待 CI；
  A19 全故障矩陣及 Win11 普通帳戶實跑仍未完成。
- run `36904848916` 已完成：兩個 workspace-boundary 與 cross-version job 均 success；
  兩個主 test job 唯一失敗 step 是 gateway rejection。此結果不替代 Win11 證據。

### A17 subagent 結果來源與順序加固

- verifier 原只用全域收集的 received；只有 child request + 預置 received 就可能
  通過，未證明父模型真的收到子結果。新增這個反例先 AssertionError not raised
  RED，再要求最後 child request 之後的非 child system request 內，實際存在
  acceptance_0、無 is_error、含 child marker 的 tool_result；child echo 不算父結果。
- 完整 Python 107 tests：104 pass／3 Windows-only skip；diff-check pass。
  不將這項 verifier 加固當作實際 subagent 成功證據，仍須 CI 與 Win11 實跑。

- `fcf7b3b` / run `36908228201`，Windows job `110524065191`（2.1.282）：
  校驗過的未修改原生引擎子代理診斷 exit 0、invalid_lines 0；固定旗標為
  init_task=true、init_agent=false、emitted_agent=true、emitted_task=false、
  emitted_unregistered=true。獨立 frontend 子代理案例仍以 E_TOOL_UNKNOWN 失敗。
  依此實際證據加入 Task 宣告→Agent 事件的單向相容映射；未宣告 Task/Agent 時
  Agent 仍拒絕，其他未知工具不放寬。新增 C++ 測試先 RED（E_TOOL_UNKNOWN）後 GREEN；
  七組 C++ suite 通過，Python 108 項中 105 通過／3 Windows-only skip。
  尚須後續 Windows 真實 frontend 子代理成功證據；原生診斷不替代 A17 驗收。
  同一 job 的 gateway TLS 仍失敗：兩次 tls-record、transport errno 10054、HTTP 0，
  不足以證明憑證拒絕；A18 不標記通過。

- `73b1d9c` / `36909430035` 已完成但失敗：2.1.282 job `110528094167`
  的子代理已不報 E_TOOL_UNKNOWN，仍以 E_PROTOCOL_ORDER 退出 65；收到的原生
  tool_result 是背景代理啟動 metadata，不是子代理結果，因此不得將啟動等同完成。
  兩版本子代理 gate 均失敗。新增唯讀、固定欄位的原生事件順序摘要（result_count、
  assistant_after_result），用於確認背景事件／result 的實際順序，不放寬 frontend
  complete 後拒絕事件的規則，也不把背景啟動 metadata 當成功結果。
  該 run cross-version 與兩個 workspace-boundary job 成功；2.1.221 payload gate
  另有失敗，尚需獨立定位；TLS gate 仍失敗。整體不放行。

- `effe8c0` / `36910938078` 兩版本原生順序摘要均為 result_count=2、
  assistant_after_result=true、exit_code=0、invalid_lines=0。這證明背景子代理
  有第一個 result 後的事件，尚未足以定義可信 task-completion 狀態機。
  新增獨立明示 run_in_background=false 的前景案例，必須在真實 API schema
  宣告 boolean 控制才執行；原始預設／背景案例仍保留為 required gate，不能用
  前景通過代替背景完成。Python TDD RED→GREEN；110 項 107 通過／3 skip。
  2.1.221 job `110533125931` payload gate 失敗觀察為 E_EXTRACT、request_count=0，
  尚不能歸因於 gateway 或容許重送。TLS／背景子代理仍未通過。

- `50b0499` / `36912651148` 已完成 failure；兩版本 job `110538863528`、
  `110538863687` 均明確打印前景案例 PASS：獨立 child system context、子代理結果
  進入後續 parent model request、來源不變、歷史只進 data。僅證明明示前景
  deterministic fixture；預設背景仍 E_PROTOCOL_ORDER，A17 整體仍未通過。
  新增固定 allowlist 的原生 task lifecycle 計數（task_started、task_notification、
  completed／failed notification、帶 parent_tool_use_id 的 assistant），不輸出
  task ID、summary 或未知 subtype，不放寬產品事件順序。TDD RED→GREEN，
  Python 111 項 108 通過／3 Windows-only skip；後續 CI 須核實實際事件 schema。

- `604ee05` / `36914155384`，2.1.282 job `110543872014` 原生診斷：
  task_started=1、task_notification=1、completed_notification=1、
  failed_notification=0、child_assistant=1；result_count=2、assistant_after_result=true。
  依已觀察的結構化生命週期實作 frontend 背景 task 狀態機：task_started 必須
  關聯本 turn 已驗證的 Agent/Task tool ID；存在 pending task 的 result 不是
  最終完成；匹配 task 的 completed notification 才允許後續 parent result。
  未知／重複 task、無關聯通知、無通知的重複 result 仍 fail closed；failed／stopped
  保留失敗。新增前後 result 案例先 RED（過早 complete）再 GREEN；未綁定 task
  與未知 notification 負例通過。七組 C++、Python 111 項（108 pass／3 skip）通過。
  尚須 Windows 實際 schema／背景 parent result 證據；不能以狀態機單元測試放行 A17。

- `dd08f71` / `36915876558`，2.1.282 job `110549587640`：明示前景
  PASS，預設背景仍 E_PROTOCOL_ORDER，退出 65。原生 lifecycle 計數仍顯示
  一次啟動／完成通知與兩個 result，但計數不能證明通知相對 result 的順序。
  現有 pending-task 狀態機尚未通過實際背景流程，不得記為已修復。
  新增固定 allowlist 事件順序投影，只輸出 init／task_started／task_notification／
  parent_assistant／child_assistant／result；不輸出事件內容或未知類型。
  TDD RED→GREEN；Python 112 項 109 pass／3 Windows-only skip。

- `83d1983` / `36917417609`，job `110554697675` 的實際原生順序：
  init→parent_assistant→task_started→child_assistant→task_notification→
  parent_assistant→result→init→parent_assistant→result（exit 0）。完成通知在
  第一個 result 之前，因此前一 pending-only 修復未涵蓋此序列。
  新增 matched completed task 授權的一次 parent-turn init 邊界，保留同 session
  校驗；任意 duplicate init／無授權重啟仍拒絕。C++ 新案例先 RED 後 GREEN。
  背景 verifier 獨立要求 child 後的 parent model request 含 user completion
  notification 與 child result marker，不接受只有 async launch metadata；Python
  新案例 RED→GREEN。七組 C++、113 Python（110 pass／3 skip）通過。
  該通知格式仍須 Windows integration 核實，不把本機 GREEN 當 A17 全量通過。

- `b41a596` / `36918944507`，jobs `110559845111`（2.1.282）、
  `110559844711`（2.1.221）均明確打印兩次子代理 PASS：明示前景與預設背景
  的 child context／後續 parent 結果驗證均成功，來源不變且 data history 存在。
  僅此 deterministic Windows hosted CI 範圍成功，不替代 Win11 普通帳戶、
  實際企業 gateway 或第三方完整矩陣；A17 整體仍未放行。
  兩版本 TLS gate 仍失敗（tls-record 兩次／HTTP 0／E_ENGINE），不得分類為
  憑證拒絕。新增 task ID 同 stream 不可重用負例先 RED 再 GREEN，防止已消費
  task 完成授權被重用；七組 C++、Python 113（110 pass／3 skip）通過。

- 結構化 TLS 分類補齊：frontend 僅在 assistant／system／result 的
  `error.code` 精確匹配六個 allowlisted 憑證驗證錯誤碼時停止並回報
  `E_GATEWAY_TLS`；不輸出 error.message，不從模型文字、errno 10054 或
  HTTP 0 推論憑證拒絕。18 個事件／錯誤碼組合先 RED（ExpectError 未拒絕）
  再 GREEN；模型文字包含 CERT_HAS_EXPIRED 不觸發分類。七組 C++ 通過；
  Python 113 項（110 pass／3 Windows-only skip）在允許 loopback socket
  後通過；初次 sandbox 執行的六個 socket PermissionError 不算產品失敗。
  此修改僅證明分類器能消費結構化證據；當前兩版原生引擎尚未觀察到該欄位，
  不代表 TLS gateway gate 或 Windows 11 x64 正式驗收通過。GitHub runner
  inventory 本次核對仍為 0 個 self-hosted runner。

- 企業組裝 boundary 固定 schema 型別加固：公開組裝命令的四個負例
  （schemaVersion=true／1.0、originalRuntimeNamesPresent=1、createsData=0）
  先 RED：原比較錯誤成功並產生候選。改用排序 JSON 比較，保留物件鍵順序
  無關性，同時區分 bool／int／float；四例 GREEN，均 E_BOUNDARY 且輸出
  路徑不存在。企業包／生命週期 10 項通過；完整 Python 114 項
  （111 pass／3 Windows-only skip）通過，diff-check 通過。
  僅驗證 schema fail-closed；不是 A02／A04 外部政策核准、A20 簽署或
  Windows 11 x64 實機放行證據。

- 企業 lifecycle 獨立重驗 boundary 型別：公開 inspect 負例修改候選
  originalRuntimeNamesPresent 為數值 1，再重建 ZIP 與完全匹配的 audit／hash。
  舊驗收器仍回報 passed（RED），證明 assembly 型別檢查不足以保護後續
  不可信候選。lifecycle 改用排序 JSON 型別敏感比較後 E_LIFECYCLE_MANIFEST
  （GREEN）；未以 saved audit 或 hash 一致取代語義驗證。企業 11 項通過，
  完整 Python 115 項（112 pass／3 Windows-only skip）通過，diff-check 通過。
  不把 synthetic 候選重驗成功當成簽名、合法通知或 Win11 實機驗收證據。

- A19 診斷代碼隱私加固：先 RED 證明格式合法但未知的
  E_PRIVATE_TOKEN_12345 被直接輸出。NeutralErrorCode 改為固定產品代碼
  白名單，未知代碼與帶私密 suffix 的已知代碼均降為 E_LOCAL；已知代碼仍
  去除 colon 後詳細內容。FailureDiagnostic 負例確認 errorCode／category
  降級且序列化不含 PRIVATE_TOKEN。七組 C++ 通過；Python 115 項
  （112 pass／3 Windows-only skip）通過，diff-check 通過。
  此項只補強產生器，不能取代正式 Windows 11 x64 故障矩陣的實跑 JSON。

- `524cf5e` / `36921309164`，2.1.282 job `110567777240`：payload gate
  第 2 次嘗試 request_count=0／exit64／E_EXTRACT；不是 gateway 回覆失敗。
  尚不能判定寫檔、替換、sharing 或磁碟原因。PrepareRuntime 分離
  E_EXTRACT_WRITE 與 activation 失敗；只取 failed MoveFileExW 的即時
  GetLastError，映射固定 access／sharing／lock／disk-full／unknown 類別，
  不輸出路徑或任意 numeric error。不增加 extraction retry 或模型請求重放。
  純分類測試先 RED（header 缺少）再 GREEN；fixture 六種新 exact startup
  code 先 RED（None）再 GREEN；七組 C++、Python 115（112 pass／3 skip）
  通過。僅增加可信故障定位，不代表間歇 E_EXTRACT 原因已修復。

- 企業組裝不可信 JSON 消歧：boundary／provenance 重複 schemaVersion
  （先 999、後合法值）公開命令先 RED：last-key-wins 仍組裝成功。
  新增每個 object scope 的 duplicate-key 拒絕解析，包含 nested objects，
  邊界／來源分別降為 E_BOUNDARY／E_PROVENANCE，輸出不存在（GREEN）。
  企業 12 項、完整 Python 116 項（113 pass／3 Windows-only skip）通過；
  diff-check 通過。此項不提供 A20 信任根或簽署批准；其仍需外部核准。

- 生命週期驗收 JSON 消歧：公開 inspect 命令對 saved audit 中重複 status
  （failed 後 passed）先 RED：退出 0；改用組裝端相同的逐 object
  duplicate-key 拒絕解析，降為呼叫端指定的 E_LIFECYCLE_AUDIT（GREEN）。
  此解析也覆蓋 manifest，不能用 last-key-wins 掩蓋歧義。
  lifecycle 5 項通過；完整 Python 117 項（114 pass／3 Windows-only skip）
  通過；diff-check 通過。仍不構成 A20 簽署或 Windows 11 實機證據。

### 2026-10-02 同一診斷 run 的實際結果核對

- GitHub run 36922839494（adapter 1e6e068）新版 2.1.282 job
  110572849885 已完成，唯一失敗步驟是 actual engine gateway rejection。
  Python contracts、native runtime/resume、frontend、actual tools、MCP、
  Skill、兩種 subagent、workspace aliases、payload integrity 及 session
  concurrency 均成功。payload 本次通過不證明間歇 extraction 故障根因消失。
- TLS endpoint 實際收到兩次 tls-record，均 transport errno 10054；
  HTTP request count 0，前端 exit 1／E_ENGINE，未取得 E_GATEWAY_TLS。
  native probe：error_result=true、success_subtype=true、result_text=true，
  structured_tls_code=false、retry_event=false、stderr_present=false。
  不能把 transport reset 或零 HTTP 當作憑證拒絕的充分證據，TLS gate
  保持失敗。不能以獨立預檢代替此實際 engine model request 的證據。
- 同一 run 的舊版 job 110572850175 查詢時仍 in_progress；沒有 rerun。
  GitHub runners API 當次 total_count=0，無可用 Windows 11 self-hosted
  驗收端點。以上 hosted 結果均只算補充回歸，不放行 A01–A20。
- 下一階段必須補 actual TLS 故障分類、A20 簽名 manifest／信任政策及
  Windows 11 普通帳戶實跑；核准 signer、通知／受限名稱／再分發政策
  必須由外部提供，不可自造核准。不要將私鑰寫入 repo 或聊天。

- lifecycle provenance 來源 URL 獨立重驗：將候選 payload URL 改為 HTTP，
  重打 ZIP 並重算完全匹配 audit，公開 inspect 舊版仍退出 0（RED）。
  現在沿用組裝端 HTTPS／無 URL credentials／無 fragment 規則，
  manifest 或 payload URL 不合格及 malformed URL 均降為
  E_LIFECYCLE_MANIFEST（GREEN），不進行任何 URL 網路請求。
  lifecycle 6 項、完整 Python 118 項（115 pass／3 Windows-only skip）
  通過；diff-check 通過。URL 格式重驗不提供官方來源真實性或可信簽署。

### A20 工程簽章驗證核心（尚未接入放行）

- 新增 manifest-signature.mjs，使用 Node 內建 crypto 的 Ed25519 驗證，
  不自製密碼算法、不載入私鑰。外部 signer 必須簽署 UTF-8 固定前綴
  `ccode-enterprise-manifest-v1` 加一個 NUL byte，再串接原始 manifest bytes。
  public key 使用 canonical SPKI DER；其 SHA256 pin 必須由獨立核准政策
  提供，不從候選中的 key/hash 自動建立信任。
- 正向真實簽章測試先 RED（module 缺少），實作後 GREEN。四組 Node
  測試涵蓋 byte/signature/key/pin 篡改、大小限制、缺 domain、RSA
  誤用及 DER trailing bytes；全部通過，Node v22.22.0，diff-check 通過。
  測試私鑰只在程序記憶體中生成，未写入檔案／repo。
- 此函式僅為工程元件：尚無核准 key、CLI、企業候選簽章附檔、
  launcher/update fail-closed gate、固定 Windows 驗證依賴及實機證據。
  不給 ccode.exe 新增 Node 執行需求，不宣稱 A20 已通過；下一步須
  接入完整候選重驗及簽章拒絕／回退矩陣，不能以函式測試代替放行。

- A20 工程 CLI：verify-manifest-signature.mjs 接收 manifest／detached sig／
  canonical SPKI DER／獨立 trust pin 四個參數。成功只輸出已驗證 bytes
  的 manifestSha256；拒絕只輸出 E_MANIFEST_SIGNATURE，exit 2，不洩漏
  path／raw crypto errors。regular-file checks、開啟後 identity check 及
  maximum+1 bounded read 拒絕 link／directory／oversize，不改寫輸入。
  公開命令真實檔案測試先 RED（CLI 缺少），實作後 GREEN；六組 Node
  測試通過，含缺檔／directory／過大 signature／無效 pin／缺參數。
  尚未測到 Windows reparse/race 全矩陣，不宣稱抗任意並行替換；仍須
  固定 dependency、candidate inspect 整合、啟動 gate 及 Windows 11 證據。

- A20 完整候選工程整合：enterprise_lifecycle.py 新增 inspect-signed，
  要求外部 signature／SPKI DER／獨立 trust pin，先重算完整候選 audit，
  再驗證 detached signature，並比對驗證出的原始 manifest SHA256 與
  本次解包 audit 的 manifest entry。missing runtime／timeout／無效
  signature／錯 pin／hash 不同均 E_LIFECYCLE_SIGNATURE，exit 2。
  公開命令先 RED（command 不存在），實作後 GREEN；真實 Ed25519
  簽章成功，錯 pin 拒絕，修改 manifest 並重算匹配 audit 後普通 inspect
  成功但舊 signature 被拒絕。lifecycle 7、Node 6 項通過；完整 Python
  119（116 pass／3 Windows-only skip）通過；diff-check 通過。
  此命令需要工程端 Node（本機 v22.22.0），尚須固定/核准 Windows
  驗證依賴；--node 可指定工程 runtime 路徑。未接入 launcher/update
  gate，未保護任意並行 package 替換，沒有 Win11／核准信任根證據。

- 簽章驗證 CI 工具鏈固定：官方 nodejs/node releases API 核實
  v22.23.3（2026-09-23 發布），官方 actions/setup-node v7 ref 核實
  commit 820762786026740c76f36085b0efc47a31fe5020，並讀其 action.yml
  確认 node-version／package-manager-cache inputs。兩個 Windows workflow
  固定 Node 22.23.3、setup action commit，停用 package-manager cache，
  新增獨立必要的 manifest signature contracts gate，非零立即失敗。
  workflow contract 先 RED（缺 pin）後 GREEN，14 項通過；Node 六組
  本機 v22.22.0 通過；完整 Python 120（117 pass／3 skip）通過。
  尚未取得固定 22.23.3 的 hosted/Win11 執行結果，不能把本機較舊
  runtime 的成功當成新 CI 版本證據，也不代表所有其他依賴已固定。

- Windows 11 企業生命週期驗收入口強制 signed inspect：新增 mandatory
  SignaturePath／PublicKeyPath／TrustedPin 及工程 NodeCommand；未通過
  signed candidate preflight 不建立 working root、不複製或執行候選。
  第一次 platform/provenance 執行前，逐檔核對複製結果與已簽 manifest
  所綁定的本次 audit path/size/SHA256；證據記錄 signatureVerification
  與 signedManifestSha256。原檔案搬移/外置資料/repack gate 保留。
- 結構 contract 先 RED（缺 mandatory signature/pre-execution check），
  實作後 GREEN，workflow 15 項通過。新增 Windows-only 公開 PowerShell
  拒絕測試：無效 signature/key 不得建立 working root／寫 evidence。
  本機無 Windows／pwsh，該實跑測試跳過，不能冒稱其 GREEN。
  完整 Python 122（118 pass／4 Windows-only skip）通過，diff-check 通過。
  同步更新正式驗收命令，必須使用獨立核准 pin 及預備的工程 runtime。
  此處只接入驗收 harness，未完成 ccode.exe 內建啟動/更新 gate、核准
  signer 或任意並行替換防護；A20 與 Win11 全量驗收保持未完成。

- 2026-10-02 補充遠端實際證據（非 Win11 普通帳戶驗收）：
  run 36925678448／commit 375f805 已完成且 failure。
  兩個版本的 manifest signature contracts 與 Python contracts gate
  均成功，提供固定 Node 22.23.3 工具鏈的 hosted 執行證據；不等同
  內建 launcher signature gate 或 Windows 11 驗收通過。
  舊版 job 110582295052 的 payload integrity gate 實際失敗診斷為
  attempt=1、exit_code=64、request_count=0、startup_code=E_EXTRACT_ACCESS。
  此碼來自 MoveFileExW activation 的 ERROR_ACCESS_DENIED 分類；
  目前只證明 activation 遭拒，未證明殘留程序、防毒、ACL 或其他根因。
  同 job TLS gate 記錄 connections=2、protocol_observations 為兩次
  tls-record、handshake_errors=[]、http_requests=0、exit_code=1，
  neutral diagnostics tls=false／retry=false／engine=true。
  因此不能宣稱已取得憑證拒絕或正確 TLS 分類證據。
  HEAD 29477d4 的 run 36926412194 在此次查詢時仍 in_progress，
  job 110584728848 與 110584729218 均正在執行；沒有 rerun。
  使用者提供的 run 36877451807 已成功執行編譯與多項測試，失敗
  是測試／runtime 問題，不是該 run 被 billing quota 阻擋。

- A19 DNS 結構化分類工程契約：只在 assistant/system/result 的
  error.code 明確為 ENOTFOUND 或 EAI_AGAIN 時拒絕並產生
  E_GATEWAY_DNS；模型文字中的同名字串不得觸發分類，raw host/token
  不渲染。診斷 allowlist 保留此中性碼並分類為 network。
  frontend 與 diagnostic 測試先 RED（未拒絕／碼未 allowlist），
  實作後兩組 GREEN；完整 Python 122（118 pass／4 Windows skip）。
  此為合成結構化事件契約，不證明原生 engine 在 DNS 故障時實際提供
  該欄位，不修復目前 TLS 分類缺口，A19 實機矩陣仍未完成。
- run 36926412194／commit 29477d4 的新版 job 110584728848 已完成；
  只有 actual engine gateway rejection gate 失敗，payload gate 本次
  成功，不能據此宣稱間歇性 E_EXTRACT_ACCESS 已修復。
  日誌明確列出 Windows-only unsigned candidate rejection test 為 ok；
  Python 共 122、skipped=1。這是 hosted Windows 證據，非 Win11
  普通帳戶實機證據。TLS 兩次 tls-record／transport errno10054、
  HTTP=0、exit=1、tls=false／engine=true；不推斷憑證拒絕。

- A19 checksum 分類修正：公開 FailureDiagnostic 對 E_CHECKSUM
  原先保留 errorCode 但錯分 local；新增完整 JSON 結果測試先 RED，
  修正後 GREEN，category=integrity，operationId/exitCode 保留，
  原始 payload 路徑與 token 不進報告。只作 exact code 分類，不增加
  原始字串洩漏或更改主要 exit code；Win11 實跑證據仍待取得。
- run 36926412194 已全部 completed/failure；兩個版本 job 都只在
  actual engine gateway rejection gate 失敗。cross-version 與兩個
  workspace-boundary job 均成功。舊版 job 110584729218 的 TLS
  handshake_errors=[]、兩次 tls-record、HTTP=0、exit=1，仍不能
  當成憑證拒絕證據。新版 payload 成功不消除先前間歇 activation
  access-denied 失敗，TLS 與 A15 未放行。

- A20 驗簽前 JSON 讀取上限：enterprise lifecycle 的 audit/manifest
  JSON 統一最多 1 MiB（與 manifest signature CLI 上限一致），先拒絕
  超大 regular file，並以 maximum+1 有界讀取防止 size check 後增長
  導致無界配置。超限保留 caller-selected E_LIFECYCLE_AUDIT／MANIFEST，
  不輸出原始路徑。公開 inspect 有效但超大 audit 測試先 RED（原先
  接受），實作後 GREEN；lifecycle 9（8 pass／1 Windows skip），
  完整 Python 123（119 pass／4 Windows skip），diff-check 通過。
  這不聲稱解決任意 symlink/reparse/concurrent replacement race，
  不代表內建 launcher/update 簽章 gate 或 Windows 11 全量验收完成。

- TLS 根因調查 probe 補充：只對 type=result 且 is_error=true 的
  result/error/errors 字串計算固定 certificate_text／connection_error_text
  布林旗標；不輸出原文、路徑、token 或任意字串。欄位明確命名為
  failure_text_hints_not_tls_evidence，反射文字不能當憑證拒絕證據。
  未更改 gateway acceptance 或 frontend 分類，未放寬 TLS gate。
  測試先 RED（缺 summarizer），實作後 probe 七項 GREEN；完整
  Python 124（120 pass／4 Windows skip），diff-check 通過。
  真實 engine 旗標結果需等待此版本 hosted CI；此處不預測旗標值。

- A20 candidate schema 嚴格型別：manifest 與 provenance 的
  schemaVersion 必須是 JSON 整數 1，不接受 Python 等值比較會誤認
  為 1 的 true／1.0。公開 inspect 測試建立四個實際候選並重算匹配
  ZIP/unpacked audit；原實作四例均 RED（錯誤接受），修正後 GREEN。
  lifecycle 10（9 pass／1 Windows skip），完整 Python 125
  （121 pass／4 Windows skip），diff-check 通過。
  此為格式拒絕契約，不構成 signer 核准、內建 gate 或 Win11 放行。

- A18 繼承環境不得削弱憑證驗證：官方 nodejs/node 的 doc/api/cli.md
  經 GitHub API 實讀，NODE_TLS_REJECT_UNAUTHORIZED=0 明確會停用
  TLS certificate validation。BuildEnvironment 現在於繼承及 alias
  展開後固定該值為 1，大小寫變體不能保留 0。BoundaryManifest 與
  企業組裝器 canonical boundary 同步公開這項固定值；沒有隱藏原始
  runtime 設定，也沒有改寫官方負載。
  真實 BuildEnvironment 測試先 RED（繼承 lowercase=0），實作後
  GREEN，同時核對公開 boundary；企業測試 18（17 pass／1 skip），
  完整 Python 125（121 pass／4 Windows skip），diff-check 通過。
  這只證明前端產生的環境 block 不包含該已知降級，不證明原生 engine
  全部 TLS 路徑採用 Node 語義，也不修復 TLS error 分類或替代 Win11
  實機不信任／過期憑證與企業 CA 矩陣。未更改核准 CA 或 proxy 設定。

- TLS 調查新證據：run 36928402551／7164749，新版 job
  110591327747 的 native probe 實際回報 certificate_text=true、
  connection_error_text=false、structured_tls_code=false、error_result=true、
  result_text=true、success_subtype=true，HTTP=0、exit=1。只證明
  失敗結果文字含憑證詞彙，不證明精確錯誤碼或憑證拒絕。
- 下一步診斷 inventory 採固定完整訊息精確匹配，僅對 result 且
  is_error 為布林 true 生效，輸出固定 code 或 unmatched；文字前後
  插入 prompt/token、模型事件、成功結果與數字 1 不接受。欄位標示
  canonical_certificate_message_hint_not_tls_evidence，不接入正式
  frontend/gateway 分類。inventory 為調查假設，尚非已觀察原生契約。
  測試先 RED（缺函式）後 GREEN；probe 八項，完整 Python 126
  （122 pass／4 Windows skip），diff-check 通過。待遠端確認精確匹配。

- A20 CI action 依賴固定：兩個 Windows workflow 的 checkout v4、
  setup-bun v2、msvc-dev-cmd v1、upload-artifact v4、download-artifact
  v4 均以各官方 repository git ref API 核實 commit type 及完整 SHA，
  用該 SHA 取代可變 tag，保留版本註解；既有 setup-node SHA 不变。
  未升級 action major 或 Bun/Node runtime，沒有降低測試 gate。
  workflow contract 先 RED（16 個可變 tag 引用），改動後 GREEN；
  workflow 16 項、完整 Python 127（123 pass／4 Windows skip）通過。
  這只固定 action 原始碼，不宣稱 hosted image、MSVC/SDK、Python、
  action 內部所有下載或 Windows 11 工程 runtime 已完整固定。
  實際兩 workflow 執行與核准完整工具鏈仍待驗證。

- TLS 精確訊息假設實測：run 36929606191／3754b7c，新版
  job 110595336469 已完成 failure；完整 job log 的 native probe
  回報 canonical_certificate_message_hint_not_tls_evidence=unmatched。
  certificate_text=true、connection_error_text=false、error_result=true、
  structured_tls_code=false、result_text=true、success_subtype=true；
  exit=1、connections=2、HTTP=0、invalid_json_lines=0、stderr_present=false。
  Gateway gate 實際失敗：兩次 tls-record，兩次 transport errno=10054，
  neutral diagnostics tls=false／retry=false／engine=true。
  因此固定完整訊息 inventory 尚未匹配已觀察結果，不得提升為原生
  engine 契約或正式 TLS 分類器；也不能將 reset 或 certificate 詞彙
  當成憑證拒絕證據。此 job 的其他測試及證據上傳通過不放行 TLS gate，
  不代表 Windows 11 普通帳戶驗收。此記錄僅更新實測證據，未改程式、
  未重跑工作流、未放寬 acceptance。
- 同 run 舊版 2.1.221 job 110595336068 已完成 failure，完整日誌也
  回報 canonical_certificate_message_hint_not_tls_evidence=unmatched；
  certificate_text=true、structured_tls_code=false、error_result=true、
  success_subtype=true、result_text=true、exit=1、connections=2、HTTP=0。
  Gateway 失敗快照與新版不同：protocol_observations 兩次 tls-record，
  handshake_errors 為空；neutral diagnostics 仍 tls=false／retry=false／
  engine=true。不得套用新版 errno=10054 到舊版，也不得將空陣列推論為
  TLS 握手成功；此快照未證明兩版本共同根因。下一步調查必須區分
  server handshake 觀察不足與 frontend 未取得結構化錯誤兩個問題。

- TLS server 觀察補齊：loopback fixture 只在 wrap_socket 成功返回後
  累計 tls_handshakes_completed，gateway 失敗快照與 native probe 同時
  輸出該固定數值，不發布 peer bytes／錯誤原文。不修改拒絕判定或
  frontend 分類；握手完成本身不是憑證拒絕或驗收通過證據。
  實際 Python SSL client 先拒絕 fixture 憑證，再以顯式 fixture trust
  完成握手及 HTTP；新增公開 counter 斷言先 RED（缺欄位），實作後
  GREEN。fixture 13、probe 8 通過；完整 Python 127（123 pass／4
  Windows skip），diff-check 通過。原生 engine 計數仍待遠端實測，
  不把本機 Python client 結果替代 Windows engine／Win11 驗收。

- 握手完成計數的原生實測：run 36931032510／e9c5502 的完整日誌
  顯示新版 job 110600028838 gateway 與 native probe 均為
  tls_handshakes_completed=0；gateway 兩次 transport errno=10054。
  舊版 job 110600028466 gateway 與 native probe 均為
  tls_handshakes_completed=2；gateway handshake_errors=[]。
  兩版均 connections=2、HTTP=0、exit=1、structured_tls_code=false、
  canonical_certificate_message_hint_not_tls_evidence=unmatched，正式
  neutral diagnostics tls=false／retry=false／engine=true，TLS gate 失敗。
  這證明本次服務端握手觀察存在版本差異，不證明舊版發出 HTTP、
  繞過 trust 或新版 reset 的精確根因；服務端握手完成不是 client
  憑證驗證成功的替代證據。後續必須保留版本分開調查，不能將兩版
  合併成同一個「握手失敗」結論。本次沒有修改分類或降低 gate。

- A20 候選拒絕的中性診斷修復：公開 inspect 對 audit/ZIP/unpacked
  全部重新匹配、但 publicBoundary=[] 的候選原先拋 AttributeError，
  exit=1 並輸出含程式來源路徑的 traceback。公開 CLI 測試先 RED，
  加入 object 型別檢查後 GREEN：exit=2、E_LIFECYCLE_MANIFEST、
  stderr 空且不輸出候選路徑。lifecycle 11（10 pass／1 Windows skip），
  完整 Python 128（124 pass／4 Windows skip），diff-check 通過。
  不以 catch-all 掩蓋例外，不降低候選檢查；此為既有驗收器修復，
  不代表內建啟動／更新驗簽、核准 signer 或 Win11 實機已完成。
- A20 保存 audit 政策的型別拒絕：公開 inspect 對 archive/unpacked
  restrictedNames 同為非零數字的 audit，原先進入掃描器後拋 TypeError
  並輸出 traceback。公開 CLI 測試先 RED，入口要求兩者均為 array
  後 GREEN：exit=2、E_LIFECYCLE_AUDIT、stderr 空；仍重算完整候選，
  不將保存的 passed/matched 當可信證據，不使用 catch-all。
  lifecycle 12（11 pass／1 Windows skip），完整 Python 129
  （125 pass／4 Windows skip），diff-check 通過。此不替代內建 gate、
  正式政策批准或 Windows 11 普通帳戶實機證據。

### TLS 協商版本診斷（未放行）

- `57f1e73` 的 run `36932757909` 已 completed/failure：兩個主 job
  僅 actual gateway rejection 失敗；cross-version 與兩個 workspace-boundary
  job 成功。兩個 Windows Python suite 均 129 tests／1 skip。
- 舊引擎 gateway fixture 本次成功握手 1 次、transport reset 1 次；獨立
  native probe 成功握手 2 次。新引擎兩處均成功握手 0 次、reset 2 次。
  HTTP 均 0；正式診斷仍 E_ENGINE，structured TLS code 未取得。
  不把握手完成當作客戶端信任成功，亦不把 reset 當作憑證拒絕證明。
- 新增服務端成功握手 TLS 版本固定計數（TLSv1.2／TLSv1.3／other），
  同步 gateway failure snapshot 與 native probe。僅在 wrap_socket 成功
  後計數，未知版本不輸出原文；未改變 production classifier 或驗收 gate。
- TDD：真實 SSL fixture 與客戶端 negotiated version 比對先因缺少
  tls_versions 屬性 RED，再 GREEN；完整 Python 129 tests／4 Windows-only
  skips，diff-check 通過。Windows 引擎版本計數尚待 push 後實跑。

### A15 解出負載 activation 失敗調查快照

- `745285e` / run `36934636417` completed/failure：new 2.1.282 的
  payload repair attempt 1 再現 exit 64／E_EXTRACT_ACCESS／HTTP 0。
  兩個 gateway TLS gate 仍失敗；cross-version 及 workspace-boundary 成功。
- old 2.1.221 gateway 與獨立 probe 成功握手均為 TLSv1.3 ×2；new
  兩處均成功握手 0、transport reset ×2。不據此推定客戶端 trust 結果。
- payload fixture 在 auth 未到達時新增 activation_files：僅保存 cache／
  candidate 的存在、可讀、size/hash 相符及 Windows readonly 布林；hash
  以 64KiB 分塊讀取，不输出 hash/path/contents/exception text。失敗
  snapshot 不執行重試，不改動 production activation 與原 auth gate。
- TDD：實際暫存檔完整 candidate／變更 cache／缺少 candidate 先缺函數
  RED，再 GREEN。完整 Python 130 tests／4 Windows-only skips；diff-check
  通過。間歇性 access 原因仍未證明，Windows 快照尚待觸發再現。

### TLS 精確訊息假設改由本機負載靜態內容建立

- `8f7313a` / run `36936089650` completed/failure：兩個主 job 僅 TLS
  gateway gate 失敗，payload repair 本次成功，未再現 access-denied；
  因而沒有 activation failure snapshot，不能宣稱間歇性問題已修復。
  Windows Python 均 130 tests／1 skip，cross-version／workspace-boundary 成功。
- 讀取本機既有 release/ccode-fixed/ccode.exe 的靜態內容，觀測到完整
  `Unable to connect to API: Self-signed certificate detected. Check your proxy
  or corporate SSL certificates`。這是本機負載字串，不是已取得的 actual
  result event，也未重新證明該既有負載的來源／版本。
- 只將此完整固定訊息加入診斷 inventory；不匹配 prefix、不加 API Error
  前綴、不放寬 unknown 字串、不改 production classifier。新版本機負載
  的分段字串不猜測拼接。下一步由 actual native probe 驗證假設。
- TDD exact failed-result match RED→GREEN，額外 suffix／prefix／截斷仍
  unmatched；structured TLS code 仍 false。完整 Python 131 tests／4
  Windows-only skips、diff-check 通過。A19 TLS 分類仍未通過。

### Portable cleanup sharing violation: observation without masking failure

- Rechecked d800137 run 36937510594: old-version portable contract prints its
  success marker before TemporaryDirectory cleanup fails with WinError 32 on
  the copied frontend. Forced snapshot interruption has not run at this point;
  attributing this failure to that interruption is unsupported.
- Added read-only Windows Restart Manager observation on PermissionError:
  availability, process count and whether the test process is present only.
  No PID, process name, path, service name or exception text is emitted by the
  observation. The original exception is re-raised; no cleanup retry, shutdown,
  ignored failure or production change. A zero count is not proof of no lock.
- TDD missing context manager RED, privacy/original-exception test GREEN.
  Full local Python: 133 tests, 5 Windows-only skips. Added Windows-only test
  against the actual executing Python image; its execution remains pending CI.
  Real failing frontend occupancy evidence and Win11 acceptance remain pending.

### Separate TLS 1.2 native probe; original gate unchanged

- 29c359e / run 36939129641 completed/failure: both Python jobs passed
  133 tests with one skip, including the actual Windows Restart Manager test.
  Portable contracts passed without reproducing sharing failure; no occupancy
  failure snapshot was obtained. Both main jobs still failed the TLS gate;
  cross-version and workspace-boundary succeeded.
- Added a separate native TLS probe capped at TLS 1.2, alongside the original
  default negotiation probe, to investigate handshake timing without changing
  the acceptance gate or production error classifier. Results identify the
  fixed tls12_only boolean; neither resets nor handshake completion prove trust.
- TDD: new real SSL client test failed on missing maximum_version option, then
  passed: untrusted client rejected and explicitly trusted client negotiated
  TLS 1.2. Full local Python 134 tests / 5 Windows-only skips; diff-check passed.
  Actual Windows engine comparison remains pending; A19 remains incomplete.

### Public package text audit bounds

- Directory and ZIP public text reads now use a 16 MiB + 1 bounded read and
  reject oversize content with E_PACKAGE_TEXT_LIMIT. Truncated prefixes are
  never certified as passed. Explicit opaque PE binaries retain streaming hash
  verification; the audit does not alter candidate contents.
- TDD real directory/deflated ZIP oversize fixture first incorrectly passed
  (RED), then both failed closed (GREEN). Package suite 15 tests passed; full
  local Python 135 tests / 5 Windows-only skips; diff-check passed.
- This closes the public-text allocation gap, not all adversarial archive
  resource limits or package source/notices approval; A02/A04 remain unverified.

### A20 native Ed25519 verification foundation (not startup acceptance)

- Added a native verification-only adapter with strict 32-byte public-key and
  64-byte signature lengths. It links Monocypher core and optional Ed25519 C
  sources, not Node, and exposes no product signing/key-generation interface.
- Upstream source fixed at LoupVaillant/Monocypher 4.0.2 commit
  0d85f98c9d9b0227e42cf795cb527dff372b40a4, retrieved via GitHub contents API;
  original per-file notices and LICENCE.md retained, BSD-2-Clause chosen.
  PROVENANCE.json records exact downloaded source/notice SHA256 values.
  Vendoring does not constitute independent cryptographic or legal approval.
- TDD native RFC8032 empty-message known-answer test failed on missing adapter
  (RED), passed after implementation (GREEN), alongside changed message/key,
  changed/truncated/missing signature and missing key rejection. Native C99/C++17
  local compilation passed; full Python 135 tests / 5 Windows-only skips.
  Windows build now compiles/runs this native suite; MSVC result remains pending.
- No launcher signature gate or approved pin is claimed yet. Domain-separated
  manifest/SPKI/pin validation, immutable candidate handling, startup/update
  integration and approved signer remain required before A20 acceptance.
- Actual native TLS1.2 probe in 86ad2f0 run 36940414782: old engine completes
  two TLS1.2 handshakes as well as two default TLS1.3 handshakes; new engine
  completes zero in both modes. HTTP zero and exit1 throughout; structured TLS
  code false and exact-message inventory unmatched. TLS1.3 timing alone does
  not explain the old-engine result. Gates remain failed, not weakened.
- GitHub self-hosted runners API still returns total_count=0. Actual Win11 x64
  ordinary-account acceptance remains unavailable and is not marked passed.

### Native manifest signature contract and newly blocked CI

- Added native exact manifest/domain/NUL verification, canonical 44-byte
  Ed25519 SPKI prefix validation and independent lowercase SHA256 signer pin.
  Empty/over-1MiB manifests and invalid signature/key/pin forms fail closed.
  Windows uses BCrypt SHA256; macOS engineering tests use system CommonCrypto.
  No candidate-derived pin is promoted to approved policy.
- TDD missing VerifyManifestSignature RED, then independently generated Node
  RFC8032 test-key domain signature GREEN. Rejects changed manifest bytes,
  absent/uppercase/wrong pin, signature without domain, invalid/extra DER even
  when the mutated DER's own digest is supplied as the test pin. This demonstrates
  structural rejection rather than merely a pin mismatch. Native tests and full
  Python 135 tests / 5 Windows-only skips passed; diff-check passed.
- b09a700 run 36941994105 completed/failure with zero steps in every job.
  Check-run 110635402504 annotation explicitly states jobs were not started
  because recent account payments failed or spending limit needs increasing.
  This NEW billing block does not reattribute earlier executed-test failures.
  No rerun performed; MSVC native verifier remains unverified.
- Startup/update integration, approved public signer policy, candidate locking,
  signed schema inventory validation and real Win11 x64 acceptance remain open.

### 2026-10-02 billing restored: executed Windows CI evidence

- User restarted run 36942361219 at commit 9d51e45 after resolving spending
  limit. Authoritative final status is completed/failure; no agent rerun.
- Both 2.1.221 and 2.1.282 jobs executed successfully through MSVC build,
  native manifest signature contracts, Python contracts, native runtime/resume,
  portable frontend, actual engine tools/MCP/Skill/subagent, workspace aliases,
  embedded payload tamper rejection and session concurrency. Native signature
  helpers are now MSVC-tested, not merely macOS-tested; this does not establish
  startup/update signature enforcement or an approved signer.
- Both workspace-boundary jobs and cross-version job passed. Cross-version
  logs report real 2.1.221 -> 2.1.282 -> 2.1.221 history/data preservation,
  rollback activation, incompatible profile refusal and relocation checks.
- Both primary jobs failed only actual gateway TLS rejection acceptance.
  Old engine: two completed default TLS1.3 handshakes and two TLS1.2-only
  handshakes. New engine: zero completed handshakes in both probe modes;
  gateway fixture reports transport resets (10054), not certificate alerts.
  Both engines: HTTP requests zero, exit 1, structured TLS code false,
  certificate-text hint true and exact-message inventory unmatched. These
  observations do not prove a distinct neutral certificate failure cause.
  TLS acceptance remains failed; no assertion or trust policy weakened.
- Hosted Windows CI is not Windows 11 x64 ordinary-account real-machine
  acceptance. A20 integration, approved signer, remaining failure matrices
  and Win11 evidence remain open; overall delivery is not accepted.

### Native authenticated-document parser foundation

- TDD added native manifest parser/duplicate-key contract: compilation failed
  for missing parser (RED), then native signature suite passed (GREEN).
- Parser limits original bytes to 1MiB and nesting depth to 32, rejects malformed
  JSON, non-object roots, trailing input and duplicate keys at every object
  scope. Exceptions expose only E_MANIFEST_DOCUMENT, not candidate contents.
  Native regressions cover nested duplicates, sibling object scopes and limits.
- This is a parser foundation only, not full inventory/schema validation,
  signature authentication, candidate file locking or startup/update enforcement.
  These remain required before A20 can pass. Local C++17 suite and diff-check
  passed; MSVC execution of this new parser remains pending.

### Native manifest file-entry contract

- TDD missing ValidManifestFileEntry RED, then native suite GREEN. Entry shape
  is exactly path/size/sha256; size must be a nonnegative integer (not bool,
  float or text), hash exactly lowercase hex64. Paths allow only ccode.exe,
  docs/usage.md or one notice basename; traversal, nested/absolute paths,
  streams, separators, controls, trailing dots/spaces and basic Windows device
  stems fail closed. Native positive/negative regressions and diff-check pass.
- Entry validation is not whole-inventory validation: duplicate/case aliases,
  complete required file set, provenance/boundary matching, filesystem handles,
  startup/update integration and formal signer approval remain outstanding.
  MSVC verification pending; no Win11 acceptance claim.

### Native complete static inventory consistency

- TDD missing inventory validator RED -> native suite GREEN; separate ASCII
  case-alias regression failed at runtime RED -> folded-path rejection GREEN.
- Enforces sorted unique file/notice paths, required executable and usage,
  nonempty notices, exact file count, notice-to-file byte-schema equivalence and
  executable-to-file equivalence (canonical dumps preserve integer/float types).
  Missing files, mismatched notices, reordered/duplicate entries and executable
  float substitution regressions pass. Local native suite/diff-check pass.
- Unicode Windows path identity still requires platform-aware handling; ASCII
  folding is not claimed to solve all Windows filename aliases. Root schema,
  provenance/runtime boundary, locked filesystem checks and actual launcher
  enforcement remain open. No A20 or Win11 acceptance claim; MSVC pending.

### Native root/public-boundary contract

- TDD missing root contract RED -> native suite GREEN. Requires exactly the
  builder's root keys, integer schema1/build22000, Windows/x64, nonempty version,
  exact dynamic-data exclusions and unasserted redistribution marker. Public
  boundary must exactly match opaque executable, all scanned notice paths plus
  usage/manifest, and excluded parents. Integrates static inventory validation.
- Tests reject unknown/missing fields, float schema/build, wrong architecture,
  omitted scanned notices, extra boundary fields and invented approval. Local
  C++17 suite/diff-check pass; MSVC pending. Provenance and runtimeBoundary are
  currently only required to be objects and remain separate validation work.
  This helper is not startup enforcement or A20/Win11 acceptance.

### Native provenance and runtime-boundary contract

- TDD missing provenance validator RED -> native suite GREEN. Combines root
  and inventory validation with exact provenance keys/schema/package, adapter
  revision/hashes, positive integer payload size and engine-version agreement.
  Official manifest/payload URLs must match the launcher's fixed GCS distribution
  base and version segment; candidate host/query substitution is rejected.
  runtimeBoundary canonical dump must exactly equal compiled BoundaryManifest,
  including integer types. Regression tests reject bool/zero/float sizes,
  version mismatch, uppercase revision, unknown fields, altered boundary types
  and URL queries. Local native suite/diff-check pass; MSVC pending.
- Metadata claims alone do not prove candidate executable embedded provenance,
  payload hash or immutable filesystem identity. Connecting these validators to
  authenticated locked candidate files and startup/update remains required.
  A20 and Windows 11 x64 acceptance remain incomplete.

### Composed native authenticated manifest document

- TDD missing AuthenticateManifestDocument RED -> native suite GREEN. Public
  composition verifies domain-separated original bytes and independent pin
  before parsing, then enforces root/inventory/provenance/runtime boundary.
  Fixed errors separate signature, document and schema failures without raw
  contents. Regression authenticates a complete fixture, rejects changed raw
  whitespace, correctly signed wrong architecture and duplicate-key documents,
  and proves unauthenticated malformed JSON fails at signature first.
- Fixture signing uses the public RFC8032 test seed only. Production API is
  verification only; no fixture key is approved policy. Existing independent
  RFC/domain known-answer tests remain in the suite. Native suite, full Python
  135 tests (5 Windows-only skips), and diff-check pass; MSVC pending.
- This composition has no filesystem side effects and still requires approved
  compiled signer policy, locked file hash/identity checks, embedded provenance
  matching and launcher/update integration. A20/Win11 acceptance not claimed.

### Windows locked candidate-file foundation (platform verification pending)

- Added test-first LockedCandidateFile header: missing-header compilation RED,
  portable native suite GREEN after addition. This does NOT mean Win32 behavior
  passed locally: actual Windows tests are conditional and must execute in MSVC
  CI. They exercise repeated bounded reads, concurrent writer/delete denial,
  preexisting writable handle refusal, hardlink refusal and release cleanup.
- Windows implementation retains a read handle sharing only reads, opens final
  component without following reparse points, rejects non-disk/directory/reparse
  or multi-link identities and bounds allocation before reads. Fixed errors only.
- This locks a single file only: parent directory replacement/reparse aliases,
  immutable whole-candidate hash and metadata verification, approved signer and
  launcher integration remain open. Not an A20 or Win11 acceptance claim.
  Local portable native suite and diff-check pass; Windows branch unverified.

### Manifest versus embedded provenance comparison

- TDD missing embedded-provenance comparator RED -> native suite GREEN. Requires
  full valid manifest plus exact embedded metadata projection including Windows
  and x64, with canonical integer type preservation. Rejects changed payload or
  official manifest hashes, float size, wrong architecture, missing revision and
  unknown metadata fields. Local native suite/diff-check pass; MSVC pending.
- API explicitly requires resource metadata from the locked executable; this
  comparator alone does not obtain it, load the embedded payload or authenticate
  filesystem bytes. Actual extraction/hash/identity integration remains open.
  No startup/update or Win11 acceptance claim.

### Streaming SHA256 for retained candidate handles

- Test-first missing stream-hash header RED -> known SHA256 fixture GREEN;
  empty-message known answer and oversized-reader rejection pass. Fixed 64KiB
  buffer, BCrypt streaming on Windows/CommonCrypto engineering tests on macOS,
  fail closed on unsupported platforms. No full-executable buffer allocation.
- LockedCandidateFile now resets and hashes through its retained read handle,
  requires all recorded bytes to be read and rejects premature EOF. Windows
  fixture asserts digest and repeat bounded reads; actual Windows path remains
  pending CI, not proven by macOS tests. Local native suite/diff-check pass.
- Earlier 99f34f6 run 36965235632 reports both MSVC build steps success (build
  invokes native suite including conditional Windows handle tests). Full job
  log API was temporarily 404 while jobs running; no complete run claim.
- Hash generation alone does not authenticate expected hashes or lock parents;
  complete candidate verification and launcher integration remain required.

### Retained-handle manifest size/hash comparison

- Continued the existing missing-API RED test: matching observed size/hash is
  accepted, wrong size or digest rejected. Implemented schema-validated exact
  comparison; local native suite is GREEN and diff-check passes.
- LockedCandidateFile.Verify checks size before hashing, then compares the
  streaming digest from the retained handle. Mismatch uses a fixed neutral error.
  Added conditional Windows fixture assertions for success and size mismatch;
  these new Win32 assertions have not run locally and await MSVC CI. No Windows
  behavioral RED/GREEN or Windows 11 acceptance is claimed.
- Restarted run 36942361219 is completed/failure; current runs 36965632548,
  36965446929 and 36965235632 remain in progress at this check. No rerun requested.
- Parent-directory identity, complete candidate locking, embedded resource
  authentication and mandatory launcher enforcement remain outstanding.

### Authenticated complete listed-file ownership

- Missing orchestrator header RED -> native suite GREEN. Added an owning
  AuthenticatedCandidateFiles composition: authenticates original bytes and
  schema before invoking an opener, acquires all listed file handles before
  content verification, verifies every listed entry, retains them until release.
- Portable fixture tests prove all listed files remain owned on success,
  invalid signature invokes no opener, a tampered notice rejects the candidate,
  and failed verification releases the acquired handles. These are orchestration
  tests, not evidence of Windows filesystem locks or production signer approval.
- Native suite/diff-check pass; Python suite 135 tests, 5 Windows-only skips.
- Production opener still must secure candidate and ancestor identity and reject
  unlisted package files; this composition does not implement those requirements,
  load embedded resources or enforce startup. A20 remains incomplete.

### Retained candidate ancestor directory handles

- Missing directory-lock header produced Windows-target compile RED. Added
  outward local-drive ancestor traversal with retained directory handles that
  omit delete sharing; rejects non-directories, reparse attributes, relative /
  device / UNC namespaces and dot/trailing-dot/trailing-space components rather
  than normalizing traversal across unverified ancestors. Fixed neutral errors.
- New Windows fixtures attempt rename of candidate and parent while retained,
  require successful rename after release, and reject file/relative/traversal
  paths. Actual execution awaits MSVC CI; compile RED is not behavioral RED.
- Standalone header Windows cross-target syntax check passes. Full-suite MinGW
  syntax check is unavailable with the local older BCrypt headers (missing
  BCRYPT_SHA256_ALG_HANDLE); no fake SDK constants were used. macOS native suite
  and diff-check pass, but cannot prove these Windows-only assertions.
- Prior run 36965632548 is completed/failure: job 110708658490 log confirms native
  suite passed; TLS handshake acceptance still fails. Cross-version and both
  workspace-boundary jobs passed. No Windows 11 acceptance or rerun claim.
- Directory handles do not prohibit adding children, lock file bytes or enforce
  launcher startup. Complete enumeration, nested static directory locking,
  embedded resources and actual production gate integration remain required.

### Correction: directory metadata-only handles did not block rename

- Run 36966569736 at 0fbaa8f failed both native builds at the assertion that
  renaming the retained candidate directory must fail. This is authoritative
  behavioral RED, contradicting the earlier intended lock property; downstream
  artifact failures are consequences, not quota failures.
- Changed the desired directory access from metadata-only FILE_READ_ATTRIBUTES
  to FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES so directory read access
  participates in share-access checking. Kept the failing rename assertions.
  Local standalone header syntax checks do not prove the fix; Windows behavioral
  GREEN remains pending the next CI run. No parent-lock acceptance is claimed.

### Integrated Windows fresh-candidate verifier

- Added LockedCandidatePackage composition: retains root/ancestor directories,
  reads bounded manifest.json through its own retained handle, authenticates
  original bytes, retains docs/notices directory handles, acquires and verifies
  every signed static file, and checks exact enumerated paths before and after
  file acquisition. Unexpected files/directories and missing paths reject.
- Header-missing compile RED preceded implementation. Conditional Windows
  tests assemble a complete signed synthetic candidate, assert manifest/file
  deletion and directory rename denial while retained, reject an extra root file
  and a same-size tampered notice. These require CI execution; portable native
  GREEN cannot prove Windows behavior. Python 135 tests/5 skips and diff-check
  pass. The ancestor sharing fix b1f75c7 is still pending behavioral GREEN.
- Scope is a fresh static candidate, not a running program tree with generated
  runtime. Inventory checks are snapshots: they do not prevent an attacker
  inserting an extra child after enumeration. No atomic-directory safety claim.
  This composition still lacks embedded payload/metadata resource verification,
  compiled approved signer policy and mandatory launcher/update enforcement.

### Mandatory candidate embedded resource verification

- Windows-target compile RED reported the missing VerifyEmbeddedResources method.
  LockedCandidatePackage now invokes it before successful construction, while
  all listed static files and directories remain retained. Loads the PE with
  DATAFILE_EXCLUSIVE | IMAGE_RESOURCE only, never runs candidate code or imports.
- Bounded resource 102 JSON is parsed with duplicate-key/depth protection and
  matched exactly to authenticated provenance. Resource 101 size and streaming
  SHA256 must match that provenance; mismatch/missing resources fail closed.
  Resource image mapping is released after verification, file handles stay held.
- Windows build now passes its actual built PE and independent generated metadata
  to the native test. The complete candidate fixture uses that real executable,
  signs its file inventory and validates resources; separately signed conflicting
  provenance must fail. Local native/Python135 (5 skips)/diff-check pass, but
  these Win32 resource assertions await CI. No local behavioral GREEN claim.
- b1f75c7 run 36966909891 reports both Windows build steps success, confirming
  execution of the unchanged directory rename assertions after the access fix.
  Whole workflow remains in progress, not accepted as Windows 11 evidence.
- This is fresh-candidate verification, not yet an enforced launcher/update gate.
  Approved compiled signer policy, PE machine/header validation and final Win11
  ordinary-account acceptance remain open. Snapshot enumeration still does not
  prevent extra-child insertion after enumeration.

### Bounded AMD64 executable header gate

- Missing PE-contract header RED -> portable native GREEN. Header gate requires
  MZ/PE signatures, bounded nonnegative e_lfanew, AMD64 machine, PE32+ optional
  magic, executable/non-DLL characteristics and bounded optional/section headers.
  Rejects ARM64, PE32, DLL, truncated header, missing optional header and section
  table beyond EOF. It is not a full PE parser; OS data-only loading remains.
- Added retained-handle ReadRange (maximum 4096 bytes, overflow-safe recorded-size
  bounds, exact reads). Windows-target missing-method RED observed; runtime range
  assertions await CI. Candidate now checks headers on the already-held executable
  handle before loading resources; it does not reopen a path for these reads.
- Added signed ARM64 fixture with matching executable hash/inventory: candidate
  must reject with E_MANIFEST_PE before resource loading. Windows behavior pending.
  Local native/Python135 (5 Windows-only skips)/diff-check pass.
- Prior real-resource run 36967369111 reports both build steps success; these
  execute actual built-PE candidate/resource/provenance rejection tests. Whole
  workflow remains in progress; this is Windows Server supplemental evidence,
  not Windows 11 ordinary-account acceptance. Startup/update enforcement and
  approved signer policy remain incomplete.

### Native compiled signer-policy generation foundation

- Missing generator RED -> matching explicit pin produces deterministic C++17
  public SPKI/pin constants. Canonical Ed25519 DER only, exact 44 bytes; compares
  SHA256 against the explicitly supplied lowercase 64-hex pin. No automatic
  pin derivation, environment fallback, private-key input or runtime override.
- Negative fixtures reject mismatched/uppercase pins, noncanonical/oversized DER,
  hardlinked input and existing output; sentinel remains unchanged. Generated
  header byte round-trip and native compile/run smoke test pass. Full Python
  suite 139 tests, 5 Windows-only skips; diff-check passes.
- Consistency verification cannot establish organizational approval. RFC8032
  public fixture remains engineering-only. This generator is not yet wired into
  build.ps1/launcher: approved real signer material and mandatory enterprise
  startup/update gate still remain open. No A20 release claim.

### Native enterprise bootstrap and installed inventory mode

- Missing bootstrap header compile RED -> portable native GREEN. NativeEnterpriseGate
  obtains its public key/pin only from a build-time policy type, reads exactly
  bounded detached signature from fixed manifest.sig through a retained handle,
  then owns the complete candidate verification result for its lifetime.
- Installed mode allows only the signature control file and an existing runtime
  directory additionally; retains the non-reparse runtime directory before
  pruning its dynamic subtree. Fresh mode still rejects runtime residue. Other
  extra program files/directories are not allowed. Snapshot checks are not an
  atomic insertion defense; dynamic runtime contents are not authenticated here.
- Windows tests use the actual built PE, engineering public policy, valid signed
  candidate and empty existing runtime; require retained signature deletion denial,
  missing/bad signature refusal and fresh-runtime rejection. Actual Win32 behavior
  is pending CI. Native/Python139 (5 skips)/diff-check pass locally.
- Signature install convention documented in design; launcher/build/lifecycle
  entry points are NOT wired yet. Therefore normal launches do not yet enforce
  this bootstrap, and no startup-gate/A20 completion is asserted.

### Enterprise launcher/build entry-point integration (engineering slice)

- Supplementary wiring checks first failed because launcher had no startup gate
  and build had no explicit signer parameters; after integration both pass.
  These source checks do not prove Win32 behavior.
- build.ps1 accepts paired SignerSpkiPath / ApprovedSignerPin, generates a fixed
  header and links Monocypher with CCODE_ENTERPRISE_REQUIRED only for that
  explicit enterprise build. Omitting both deliberately builds the public
  edition, not an unsigned fallback inside an enterprise executable.
- Enterprise launcher retains NativeEnterpriseGate on the Main stack before
  permission worker, help, metadata and normal option parsing/side effects.
  No environment or CLI option can turn this compiled gate off.
- Every build also compiles an ephemeral enterprise launcher using independent
  RFC8032 public test material (never the output edition's trust policy), and
  runs real subprocess refusal tests for missing manifest.sig across help,
  version, package/boundary metadata, self-test, permission worker and print.
  Requires exit64, E_MANIFEST_FILE, empty stdout, no program-tree changes and
  bounded termination. Actual Windows execution of this new test is pending CI.
- Approved signer, valid-signed enterprise launcher subprocess coverage,
  installation of signature, external-data enforcement, retained runtime-use
  integrity and Win11 ordinary-account acceptance remain incomplete. This is
  not an A20/startup acceptance completion claim.

### Enterprise startup execution coverage follow-up

- Run 36969406725 (1cbb6eb): both engine-version Build reported runtime version
  steps succeeded (jobs 110720026990 / 110720027138). This includes compiling the
  real enterprise frontend and executing missing-signature refusal tests for
  all seven tested entry points. It is hosted Windows engineering evidence,
  not Win11 ordinary-account evidence or an overall green workflow claim.
- Added separate --enterprise-launcher mode to the native fixture suite and
  wired build.ps1 to execute it against the ephemeral enterprise PE. The suite
  signs a complete package from that exact executable's observed size/hash and
  independent build metadata, then actually spawns help/version/package/boundary/
  self-test. It requires success with no data creation and no runtime extraction.
- Negative subprocesses require refusal for an invalid signature (including
  permission worker and normal print), notice tampering and even a whitespace
  change to the signed raw manifest. Restoring exact bytes must restore success.
  Child processes have bounded waits and test handles are closed.
- New invocation wiring test RED -> GREEN locally; portable native suite passes.
  These Windows-only positive/tampered subprocess cases are pending the next CI
  execution. Engineering RFC8032 signing does not imply real signer approval.
  External-data, installed-signature lifecycle, runtime retention and all actual
  Windows 11 acceptance gaps remain open.

### Enterprise external persistent-data boundary

- Run 36969869332 (350c22f) both Build reported runtime version steps succeeded,
  including the valid-signed enterprise launcher subprocesses and invalid
  signature/notice/raw-manifest mutation refusal tests. Hosted Windows evidence
  proves this tested integration, not Windows 11 ordinary-account acceptance.
- Missing enterprise-data.hpp native compile RED -> portable native GREEN.
  Enterprise launcher now constructs LockedEnterpriseData before persistent
  data/profile creation; historical program/data/cc default is rejected rather
  than silently redirecting or polluting the signed program tree.
- Requires disjoint program/data roots (equal, descendant and ancestor rejected;
  component comparisons are case-insensitive, not string-prefix matches).
  Existing ancestor chains are locked as non-reparse directories before any
  creation. Compare handle-derived normalized local DOS paths to resolve caller
  spelling aliases; reject unsupported namespaces. Create missing components
  one at a time and retain each new chain before descending, never recurse
  through an unchecked concurrently inserted intermediate directory.
- Holds final data/ancestor chains until Main returns. This is root identity
  retention, not a guarantee that all data descendants cannot be modified.
  Existing profile/session integrity safeguards still apply separately.
- Windows tests require nested external data creation and ancestor/root rename
  denial until guard release. Signed launcher tests require default/equal/
  child/ancestor rejection without program data writes, and successful sessions
  initialization in an explicit sibling external root with no engine extraction.
  New Win32 behavior is pending CI. Header Windows cross-target syntax passes;
  portable behavior tests pass. No new Win11 acceptance claim.

### Detached signature installation in lifecycle harness

- Missing install-signature CLI RED -> valid-signature installation GREEN.
  enterprise_lifecycle.py installs only the fixed program/manifest.sig control
  file, independently of the fresh-package whitelist. Exclusive create refuses
  existing files/sentinels, with no overwrite. Wrong pin, invalid signature and
  hardlinked source refuse without creating the control file.
- Installer bounds manifest/signature/SPKI reads and rejects nonregular/link/
  reparse/hardlink inputs and existing non-directory/reparse root ancestors.
  Verifies buffered snapshots, compares hashes of the exact manifest/signature/
  public-key buffers verified by Node, then installs those buffered signature
  bytes. Opt-in --snapshot-evidence leaves the existing verifier CLI report
  unchanged for inspect-signed callers. Rechecks manifest snapshot before write,
  flushes/fsyncs the control file. Native startup remains the authoritative
  retained-handle inventory/trust verifier; harness ancestry inspection alone
  is not an atomic concurrent-parent-replacement guarantee.
- Acceptance harness installs before its first platform/candidate command,
  checks signed manifest hash against inspected candidate, and records only
  installedSignatureSha256. Subsequent before/after program inventories include
  this fixed control file; fresh repack still uses the original whitelist.
- Lifecycle wiring source test RED -> GREEN; real Ed25519 fixture installation,
  exclusive sentinel preservation and negative cases pass locally. JS signature
  contracts 6/6 pass. Actual Win11 lifecycle execution and production signer
  approval remain unverified, not marked passed.
- Run 36970426670 (108afe8) both Windows Build reported runtime version steps
  succeeded, including new external-data root retention and signed-launcher
  overlap rejection/sibling-data success tests (jobs 110723052645/110723052770).
  Full local Python suite now 144 tests passes with 5 Windows-only skips.

### Retained runtime executable through actual engine use

- Missing retained-runtime.hpp compile RED -> portable native GREEN for exact
  observed size/hash matching and mismatch refusal. PrepareRuntime now returns
  an owning RetainedRuntimePayload rather than a bare path, for both public and
  enterprise builds. All four RunTurn call sites use Path() while retaining the
  owner across normal print, interactive turns and candidate/rollback probes.
- Before the preparation lock is released, acquire the engine's entire existing
  parent directory chain and a regular/nonreparse/single-link FILE_SHARE_READ-only
  engine handle, then independently stream-hash that same retained handle against
  embedded resource size/hash. Hold until the full caller scope completes.
  Post-verification writes/deletion and ancestor replacement cannot occur while
  those handles are held. This is not a claim that earlier extraction writes
  are already secured against concurrent intermediate-file replacement.
- Windows native tests require mismatch refusal, write/deletion/ancestor-rename
  denial while retained, and deletion after release. Signed launcher suite also
  starts an actual PE while a RetainedRuntimePayload owns its read handle and
  parent locks, testing loader/share-mode compatibility. Existing real engine
  functional workflows will exercise the new owner end-to-end. Added BCrypt
  linkage for native test suites. New Win32 execution is pending CI.
- Local portable native/signature tests pass, Python145 passes (5 Windows-only
  skips), diff-check passes. Run36971075188 (6f2505c) both Windows build steps
  succeeded (jobs110724974170/110724974388); no full-workflow/Win11 acceptance claim.
- Runtime preparation temporary-file safety, interruption/disk-full/sharing fault
  matrix, approved signer and actual Win11 ordinary-account evidence remain open.

### 2026-10-02 — retained runtime staging and preparation ancestors

- Reconfirmed RED: native suite fails to compile without runtime-staging.hpp.
  Added regular/disk/non-reparse/single-link staging observation requirements.
- PrepareRuntime now retains program/runtime/hash ancestor chains and creates
  runtime directories one component at a time, with revalidation under locks.
  Staging opens engine.new without truncation, verifies its opened object before
  writing, flushes, and activates through FileRenameInfo while retaining the
  source handle. No close-then-path-based source rename remains.
- Writer closes before RetainedRuntimePayload is acquired. This is not an atomic
  writer-to-reader handoff: final retained hashing detects substituted bytes
  before execution; concurrent interference can still cause safe failure.
- Added Windows behavior tests for stale staging truncation, exact resulting
  payload, deletion denial, hardlink sentinel preservation, target sharing
  conflict preserving original bytes, and successful activation after release.
  Their actual execution is pending Windows CI, not proven by local tests.
- Local portable native suite passes; staging header MinGW syntax check passes;
  Python145 passes with5 Windows skips; diff-check passes. Full A15 disk-full,
  interruption, update/rollback fault matrix remains incomplete.
- Run36971631513 at10e41a9 completed failure: builds and native runtime succeeded,
  Python default cp1252 decoding failed (fixed/pushed separately in c3ce0ab),
  and actual TLS gateway rejection still lacks the required certificate evidence.
  Neither this hosted run nor local checks establish Win11 acceptance.

### 2026-10-02 — extraction write-stage error categories

- Native test RED observed missing ExtractionWriteError; GREEN passes after
  mapping disk-full/access/sharing/locked errors and retaining generic WRITE
  for unknown errors or short successful writes.
- RuntimeStagingFile now uses that mapping at truncate/write/flush failures,
  reading GetLastError only after an actual failed OS call (not short success).
- Portable native suite and MinGW staging header syntax check pass. These are
  mapping/integration checks, not a real disk-full fault injection or full A15
  acceptance. Actual constrained-volume Windows evidence remains required.

### 2026-10-02 — staging CI regression remains RED

- Run36972781321 at75aabf8 completed failure in both Windows build jobs:
  launcher compilation succeeded, but native-tests.exe exited -1073740791.
  No assertion expression was emitted. Staging Win32 behavior is therefore
  unproven and currently a regression, not accepted implementation.
- Replace new staging fixture assertions with equivalent explicit checks that
  emit fixed E_TEST_STAGING checkpoint IDs and exit1. No fixture paths/bytes
  are emitted and no assertion condition is removed or relaxed. Portable suite
  still passes; this diagnostic change awaits actual Windows execution.

### 2026-10-02 — explicit destination sharing check before activation

- Run36973127398 at1634c04 completed RED: both versions emitted
  E_TEST_STAGING_8, proving replacement rename succeeded despite an existing
  retained target reader. Earlier reliance on rename-only sharing enforcement
  was incorrect on these actual Windows runners.
- Activate now explicitly opens an existing destination with READ|DELETE before
  replacement, validates its opened disk/non-directory/non-reparse/single-link
  object, and retains that handle until activation finishes. An existing reader
  not sharing DELETE must prevent this open; only FILE_NOT_FOUND permits the
  no-existing-target case. Sharing fixture now requires E_EXTRACT_SHARING and
  still verifies original payload bytes, followed by retry after reader release.
- Portable native tests and MinGW staging syntax pass. Actual Windows GREEN is
  pending, not inferred. This is not atomic protection against newly inserted
  directory entries; final retained hashing still precedes execution.

### 2026-10-02 — runtime open diagnostics after sharing GREEN

- Run36973432428 at1c978cc completed failure overall. Both build steps passed,
  including native staging sharing/retry/sentinel tests, and Python contracts
  passed. Actual runtime/resume, tool and other real-engine integration failed,
  predominantly E_MANIFEST_FILE; this is an active integration regression.
  Workspace boundary supplemental jobs passed; none establishes Win11 acceptance.
- Runtime retained reader now uses explicit RuntimePayload file purpose, mapping
  failed CreateFile errors to fixed MISSING/PATH/ACCESS/SHARING/FILE categories.
  Manifest files retain their prior behavior. Invalid runtime file observations
  are PATH. No OS number/private path is emitted, no retries or weaker sharing
  were added. This will discriminate causes before a speculative behavior fix.
- RED observed absent RuntimePayloadOpenError; portable native/signature GREEN
  and diff-check pass. Actual integration recovery remains pending Windows CI.

### 2026-10-02 — terminated runtime activation destination

- Run36974252597 atb758c8f completed RED: both actual runtime/resume jobs
  reported E_RUNTIME_MISSING after activation; most real-engine tool cases
  also reported MISSING, not SHARING. This rules out the suggested sharing
  retry as a justified fix for those observations. Some alias/cross-version
  cases instead reported generic activation failure.
- Inspection found the variable-length rename destination had no explicit NUL
  terminator after copied characters. Allocate an extra wchar and copy the
  terminator while retaining FileNameLength excluding it. This removes reliance
  on uninitialized buffer tail during path conversion. Causality/recovery is
  not proven until actual Windows integration runs again.
- Add actual Windows staging fixture with Chinese/space/long directory name and
  131073-byte payload, checking resulting exact bytes through a locked reader.
  Portable native GREEN, MinGW syntax and diff-check pass; Windows fixture and
  real-engine GREEN remain pending. No change to sharing or final verification.

### 2026-10-02 — bounded native fixture exception reporting

- Run36974883708 atb035cf2 completed failure: both native test executables
  exited -1073740791 without a checkpoint/code. Terminator fix effectiveness
  is not proven. The new long Unicode fixture is still unverified.
- Native suite now has a top-level exception boundary. Only bounded80-character
  E_ codes containing uppercase letters/digits/underscore are printed; all other
  exception messages become E_TEST_NATIVE_EXCEPTION (unknown exceptions a fixed
  UNKNOWN code). No raw filesystem exception/path is logged and tests still fail.
- Local native suite and diff-check pass. This is diagnostic progress against
  actual CI RED, not recovery or any acceptance completion claim.

### 2026-10-02 — access failure is observed, phase still unresolved

- Run36975227657 atda21296 completed RED: both native suites now expose
  E_EXTRACT_ACCESS rather than an opaque abort. This is not yet proof which
  open/activation failed. Add fixed staging phase markers (no paths/bytes) to
  distinguish the original fixture from long Unicode open/write/activation.
- Microsoft FILE_RENAME_INFO documentation was fetched directly: FileNameLength
  is bytes and a terminating NUL is not required. Thus the prior missing-NUL
  hypothesis must not be treated as confirmed root cause. An explicit terminator
  is retained defensively; actual runtime recovery remains unproven.
- Local portable native suite and diff-check pass. Windows phase diagnostics
  remain pending; do not weaken path/share checks to force a green result.

### 2026-10-02 — retained-directory-relative activation

- Run36975542524 at6511eb1 completed RED: both suites emitted BEGIN,
  FIRST_ACTIVATE, then E_EXTRACT_ACCESS; the long Unicode fixture was not reached.
  This localizes the current failure to first activation, not Unicode writing.
- Rename now uses fixed leaf engine.exe relative to the already validated,
  retained parent directory handle. LockedCandidateDirectories exposes a borrowed
  leaf handle with ownership/lifetime retained by the caller. Destination sharing
  verification still occurs before rename and final reader hashing still occurs
  before execution; no checks were removed. This avoids re-converting full DOS
  destination names within rename. Windows behavior remains to be verified.
- Portable native GREEN, staging MinGW syntax and diff-check pass. These do not
  establish actual Windows activation or Win11 acceptance.

### 2026-10-02 — native retained-root rename awaiting execution

- Run36975859976 at828779b completed RED at FIRST_ACTIVATE with generic
  E_EXTRACT_ACTIVATE in both versions; Win32 retained-root change did not recover
  activation. No claim is made about the exact unmapped OS error from that run.
- Keep directory-relative resolution but pass the retained root directly to
  NtSetInformationFile classic FileRenameInformation, resolving system ntdll
  exports and translating unsuccessful status with RtlNtStatusToDosError.
  Missing exports fail closed. This uses the same ordinary-account access and
  source/target handles, no elevation or weaker ACL/share policy. Win32 path
  conversion is no longer part of the actual rename call.
- Existing native activation/sharing/long-Unicode and real-engine integration
  are the RED behaviors to recover, not deleted or bypassed. MinGW staging syntax,
  portable native suite and diff-check pass. Actual Windows execution is pending.

### 2026-10-02 — close destination probe before classic replacement

- Run36976241354 at1d820b5 completed RED with FIRST_ACTIVATE followed by
  E_EXTRACT_ACCESS; no later phase marker was present, so this alone does not
  prove whether first activation or a later replacement failed.
- Existing target probe remained open through classic rename. Release that
  READ|DELETE probe after its object/type/share checks, before classic replacement,
  avoiding self-blocking target handles. Source/parents remain held and final
  retained hashing still precedes execution. Target probing is explicitly not an
  atomic reservation; no immutable target-probe-to-rename transfer is claimed.
- Add FIRST_DONE and RETRY_BEGIN markers to distinguish initial activation from
  existing-target retry. Existing reader-sharing refusal, original byte equality,
  and retry success tests remain unchanged. Portable native suite, MinGW staging
  syntax and diff-check pass; actual Windows recovery is still pending.

### 2026-10-02 — bounded runtime activation contention recovery

- Run36976678691 at8856d72 completed failure, not a billing block. Build,
  runtime/resume, workspace boundary and cross-version upgrade/rollback recovered;
  new-engine tampered-cache repair still reported E_EXTRACT_ACCESS. Gateway TLS
  rejection evidence remains insufficient and is not marked passed.
- RED: portable native compilation failed on seven missing retry-policy calls.
  GREEN: retry only ACCESS/SHARING/LOCKED before the deadline. Runtime preparation
  explicitly waits at most30 seconds; other categories and deadline expiry fail
  closed. Every retry revalidates the target, retaining source and parent guards;
  no POSIX replacement, readonly changes or integrity bypass is introduced.
- Add real Windows share-denying reader release and permanent-reader deadline
  tests. These Windows-only behaviors await CI execution; portable policy passing
  is not proof that the observed ACCESS failure was transient or repaired.
- Local portable native suite and MinGW staging-header syntax check pass.
  Python145 tests pass with5 Windows-specific skips (loopback fixtures require
  execution outside the local sandbox). Diff-check passes. Windows11 ordinary
  account acceptance, mapped-image fault coverage and TLS evidence remain open.

### 2026-10-02 — enterprise pre-signing build sidecars

- RED: new assembly-input wiring test failed and independent boundary exporter
  compilation failed because the exporter did not exist.
- GREEN: add a build-only executable using the same BoundaryManifest definition,
  and export its JSON after build checks. Copy the exact resource102 provenance
  input to the output directory before temporary build cleanup. No unsigned
  enterprise launcher invocation or startup-gate bypass is added.
- Local independent exporter compiles and emits valid x64/build22000 boundary
  without creating runtime/data files. Windows PowerShell build execution remains
  pending CI; sidecars are assembly inputs, not signer approval or acceptance.

### 2026-10-02 — opened-object assembly input snapshots

- RED: deterministic pathname replacement after lstat was accepted; a separate
  same-inode mutation before open was also accepted. Both tests now reject.
- Read through one opened descriptor, compare observed/opened identity and
  size/mtime/ctime, verify regular type, then verify descriptor metadata and
  pathname identity after reading. O_NOFOLLOW is used where available. Captured
  bytes are subsequently hashed and assembled without a pathname reread.
- This detects the tested replacement/mutation cases; metadata comparison is
  not a claim of immutable reads against all concurrent same-object writers.
  Signed manifest/runtime retained-hash checks remain required on Windows.
- Full Python regression150 tests passed with5 platform skips. Correct the
  acceptance assembly instructions to use build sidecars before signing, while
  still requiring signed launcher metadata/boundary equality and no side effects.

### 2026-10-02 — activation recovery CI and TLS trust differential probe

- Run36978281307 at728d1e1 is terminal failure only in the actual-engine gateway
  rejection steps in both versions. Both builds, tampered payload/cache recovery,
  workspace boundary jobs and cross-version job completed success. This is
  supplemental hosted Windows evidence, not Windows11 ordinary-account evidence.
- Native TLS probe shows certificate-text hints but no structured TLS code; the
  new engine aborts without a completed handshake, which alone proves neither
  certificate rejection nor correct neutral TLS classification.
- RED: explicit fixture-trust configuration test failed on the missing control.
  GREEN: add a separately labelled native run using NODE_EXTRA_CA_CERTS pointing
  only to the public fixture certificate. No validation-disable setting is added;
  ordinary untrusted runs remain unchanged. A trust-control HTTP request would
  help distinguish certificate trust from transport/configuration failure, but
  this probe is not acceptance and does not fix production classification.
- Fixture certificate has IP SAN127.0.0.1 and validity2026-09-25 to2126-09-01.
  Local Python151 tests pass with5 platform skips. Actual native differential
  result and gateway/TLS acceptance remain pending.

### 2026-10-02 — superscript device-alias parity

- RED: Python notice validation accepted all six COM/LPT superscript1/2/3
  aliases; native manifest entry assertions failed on the same paths.
- GREEN: reject those stems case-insensitively in both assembly and native
  manifest validation, including names with extensions. Normal Chinese/space
  notice names remain accepted by the assembly test. This is additional path
  alias coverage, not a complete Unicode/Windows namespace acceptance claim.
- Native signature suite passes; full Python152 tests pass with5 platform skips.
  Actual Windows11 ordinary-account path evidence remains outstanding.

### 2026-10-02 — Windows CPython cross-API timestamp correction

- Run36978965055/b28bcf5 Python contracts failed on normal package assembly with
  E_INPUT_READ. This contradicts the earlier local-only green result; Windows
  snapshot assembly is not marked passed.
- Checked CPython v3.12.10 Modules/posixmodule.c win32_xstat copies birthtime into
  ctime; Python/fileutils.c _Py_fstat_noraise passes FileBasicInfo to the conversion
  without that compatibility copy. Comparing pathname/descriptor ctime directly
  can therefore reject unchanged objects.
- RED: new cross-API stamp test failed on the missing comparison policy. GREEN:
  compare size/mtime plus explicit birthtime_ns where available, else ctime.
  Retain identity checks and same-descriptor size/mtime/ctime before/after reads.
  Both swapped-object and same-object mutation regression tests remain passing.
- Local Python153 tests pass with5 platform skips. Actual Windows assembly
  recovery is pending CI; no metadata-only immutability claim is made.

### 2026-10-02 — completed Windows regression run and TLS differential results

- Run36980191935 at7c6f953 completed failure. Both engine versions passed
  build, manifest signature contracts, Python contracts, native runtime/resume,
  frontend, actual tools/MCP/Skill/subagent, workspace aliases, tampered payload
  recovery and session writer concurrency. Both workspace-boundary jobs and
  actual cross-version upgrade/rollback passed. This verifies the Windows
  cross-API timestamp regression fix in hosted CI, not Win11 real-machine acceptance.
- Both test jobs failed only at actual-engine gateway rejection. The unreachable
  gateway case passed; untrusted-certificate classification remains unaccepted.
- Jobs110752728344 (2.1.282) and110752728444 (2.1.221) provide separate native
  trust controls. Default and TLS1.2-only untrusted probes made two connections
  and zero HTTP requests in each version. Version2.1.282 completed zero TLS
  handshakes; version2.1.221 completed two (TLS1.3/default, TLS1.2/forced).
  Explicit fixture trust completed two TLS1.3 handshakes and one HTTP request
  in each version. All probes exited1, including the trusted fixture503 control.
- Both versions returned an error result with subtype success and result text,
  but no structured TLS code. The exact canonical message inventory did not
  match. Certificate-text hints occurred only in untrusted controls; these are
  investigation hints, not production diagnostic evidence. No raw result text
  or credentials are recorded here.
- Current frontend.hpp maps structured error.code only, so the observed native
  protocol cannot currently establish E_GATEWAY_TLS through that path. Neither
  handshake completion nor absence of HTTP substitutes for certificate rejection
  evidence. Keep the acceptance assertion unchanged; investigate a trustworthy
  error source before changing production classification. No speculative rerun.
- Formal signer approval and Win11 x64 ordinary-account evidence, along with the
  remaining acceptance matrix, are still outstanding. Overall acceptance is not
  complete.

### 2026-10-02 — lifecycle JSON snapshot replacement rejection

- RED: replacing an audit JSON pathname immediately after its first lstat was
  accepted by lifecycle._json; the new deterministic regression failed because
  no LifecycleError was raised.
- GREEN: lifecycle audit/manifest JSON now uses the assembly snapshot reader,
  translating PackageBuildError to the caller's neutral lifecycle code. Add an
  optional byte limit to that reader: reject oversized initial metadata and cap
  the actual read at limit+1. Preserve file identity and before/after descriptor
  stamp checks, unique JSON keys, UTF-8 BOM compatibility and the1MiB JSON cap.
- Targeted lifecycle tests12 pass with1 platform skip; snapshot tests pass.
  Full Python154 tests pass with5 platform skips; git diff --check passes.
- This rejects the tested replacement race; it is not a claim of immutable reads
  against every same-object writer or of complete candidate-directory atomicity.
  Hosted Windows regression and Windows11 ordinary-account evidence are pending.
