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
