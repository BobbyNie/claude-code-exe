# A 方案實施與驗收證據台帳

日期：2026-09-25。**狀態：實施中，未放行。**

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
| A01 | 離線安裝 | 資源內嵌引擎；Windows 建置／啟動 | 乾淨普通帳戶、離線、服務／驅動前後差異、依賴完整性 |
| A02 | 交付名稱 | 中性入口及 help 測試 | 獨立企業交付包、遞迴名稱／公开設定／手冊掃描、更新殘留 |
| A03 | 原始負載 | build 上游 checksum；啟動資源 SHA256、解出內容逐位元組校驗 | 隨包來源清單、實際 extracted hash 證據及篡改案例 |
| A04 | 內部範圍 | ADR 明確允許引擎／使用者資料原名 | 隨包公開邊界清單、runtime 環境及 metadata 記錄、必要通知核實 |
| A05 | 資料分離 | --data-dir、獨立 profile；工具 integration 檢查程式區無 session | 更新、搬移、重新打包排除資料、程式區無 temp 全面快照 |
| A06 | 路徑一致 | runtime-paths.js 真實 mkdir/stat/讀寫/列舉/刪除/子進程 | 重命名、同工作區真實工具鏈和故障結果完整證據 |
| A07 | 執行檔 | `36123600008` 兩版本通過：空格／中文程式與工作區、前端→真正 Grep/Glob；既有自啟動測試亦通過 | 本條所列 CI 場景通過；乾淨端點證據仍依 A01 |
| A08 | 路徑邊界 | 工具 fixture 含中文／空格；`87f7065` / `36135890793` 兩版本快照／候選超過 260 字元測試通過 | 全工具／全工作區長路徑、大小寫、junction；UNC 支援界線與明確拒絕／驗證 |
| A09 | 基本工具 | tools-integration.py 驅動真實 Write/Edit/Read/Grep/Glob/Bash | 兩版本一般完整路徑工具成功與 8.3 短路徑拒絕已驗證；仍欠批准／取消、政策退出碼、實際 gateway 試運行 |
| A10 | 串流 JSON | frontend-tests.cpp 每個文字 UTF-8 byte 分片點；穩定錯誤碼、失敗不可重啟 | `0097a5a` 補工具 JSON 每個 byte 分片點、無效 UTF-8、超限、缺終止／截斷後不可復活；仍欠大小邊界、斷流／故障端到端 |
| A11 | 工具名稱 | init 宣告工具清單；空名、未知名、空／重複 ID、錯誤參數分類 | 缺終止事件、所有狀態轉移與新版本／擴展的真實事件相容 |
| A12 | 權限 | PermissionRpc 預設拒絕；interactive console；Job Object；`e22cd72` / `36124014490` 兩版本真實 Bash→PowerShell 後代在 Ctrl+Break／前端強制終止後退出，無延遲寫入 | `cacdb06` / `36129711620` 兩版本真實 console 批准／拒絕／等待批准中取消通過；`199b57b` 同 profile 故障後重啟亦通過；仍需目標普通帳戶端點證據 |
| A13 | 歷史 | --sessions、--resume、--continue、/resume；fixture 驗證請求歷史標記 | `6c92fdd` / `36131964792` 兩版本列表→選擇→兩輪→重啟 continue 通過；仍欠完整損壞資料可用性分類及穩定工作區 UUID 整合 |
| A14 | 升級／搬移 | runtime 按 hash 分區；資料不綁 runtime hash | 引擎／profile 相容 manifest、重啟／搬移／更新／回退實測 |
| A15 | 遷移 | SHA-256 快照、核驗後隔離候選；`87f7065` / `36135890793` 兩版本真實引擎在候選恢復多輪歷史，來源與備份逐位元組不變 | 單一會話驗證回執已通過兩版本 CI；仍欠完整 profile 驗證及遷移流程、重名衝突處理、原子切換、磁碟滿／中斷重試 |
| A16 | 並發 | profile-wide 排他鎖 | 故障釋鎖已由 `199b57b` 實測；仍欠同 session 單寫入、多 session 同工作區 |
| A17 | 擴展 | CLI 參數可接設定／MCP／agents | 技能、子代理、核准 MCP 真實流程及明確版本相容矩陣 |
| A18 | 網路 | gateway 設定入口；不宣稱 OS 網路隔離 | 不可達、TLS、過期憑證、429、斷流分類；不重放寫入；核准端點部署政策 |
| A19 | 診斷 | 中性錯誤碼；parser 內容不直接外洩 | 操作／追蹤 ID、分類診斷包、憑證遮罩、預設不記提示／內容的實測 |
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
| D08 | 固定 adapter／引擎／工具鏈及 mapping manifest | 目前 CI 固定兩引擎，尚無完整隨包 manifest |
| D09 | 工具 schema、錯誤分类、不猜測／不重放 | A10/A11/A18；前端驗證輸出不等於能阻止引擎內部已执行的工具 |
| D10 | 隔離候選 profile、備份 hash、原子 active 切換 | 已實作 SHA-256 快照、隔離候選及 single-session 驗證回執；尚欠完整驗證範圍、來源變更衝突與原子 active 切換／回退 |
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
