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
| A08 | 路徑邊界 | 工具 fixture 含中文／空格 | 長路徑、大小寫、junction；UNC 支援界線與明確拒絕／驗證 |
| A09 | 基本工具 | tools-integration.py 驅動真實 Write/Edit/Read/Grep/Glob/Bash | 兩版本一般完整路徑工具成功與 8.3 短路徑拒絕已驗證；仍欠批准／取消、政策退出碼、實際 gateway 試運行 |
| A10 | 串流 JSON | frontend-tests.cpp 每個文字 UTF-8 byte 分片點；穩定錯誤碼、失敗不可重啟 | `0097a5a` 補工具 JSON 每個 byte 分片點、無效 UTF-8、超限、缺終止／截斷後不可復活；仍欠大小邊界、斷流／故障端到端 |
| A11 | 工具名稱 | init 宣告工具清單；空名、未知名、空／重複 ID、錯誤參數分類 | 缺終止事件、所有狀態轉移與新版本／擴展的真實事件相容 |
| A12 | 權限 | PermissionRpc 預設拒絕；interactive console；Job Object | 批准、拒絕、等待中取消、前端異常退出真實子進程案例 |
| A13 | 歷史 | --sessions、--resume、--continue、/resume；fixture 驗證請求歷史標記 | 列表→選擇→續接 UI 路徑與多輪、損壞資料分類、穩定工作區 UUID |
| A14 | 升級／搬移 | runtime 按 hash 分區；資料不綁 runtime hash | 引擎／profile 相容 manifest、重啟／搬移／更新／回退實測 |
| A15 | 遷移 | 舊版 CopyMissing 保留來源及現有檔案；profile 測試 | 備份+hash、隔離候選、引擎驗證後原子切換、磁碟滿／中斷重試 |
| A16 | 並發 | profile-wide 排他鎖 | 同 session 單寫入、多 session 同工作區、前端故障釋鎖 Windows 實測 |
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
| D04 / R05 | 穩定工作區 UUID、會話權威檔案 | 尚無工作區 UUID registry；A13–16 |
| D05 / R06 | 核准 API 通道 | A18；普通本機封裝本身無法封鎖工具任意外連，部署政策另驗 |
| D06 / R07 | 受控完整進程樹 | A12；Job Object 結束／取消／崩潰無孤兒進程 |
| D07 / R08 | 更新、診斷、恢復 | A14/A15/A19/A20 |
| D08 | 固定 adapter／引擎／工具鏈及 mapping manifest | 目前 CI 固定兩引擎，尚無完整隨包 manifest |
| D09 | 工具 schema、錯誤分类、不猜測／不重放 | A10/A11/A18；前端驗證輸出不等於能阻止引擎內部已执行的工具 |
| D10 | 隔離候選 profile、備份 hash、原子 active 切換 | 現有 CopyMissing 僅是舊版兼容，未達設計遷移流程 |
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
  Windows 最終回歸結果待後續 run 補記。

## 下一批實施順序

1. 驗證實際工具端到端，處理揭露的缺陷；補取消／權限／進程樹及路徑邊界。
2. 工作區與 profile 生命週期：穩定身份、候選遷移、驗證／原子切換、備份／回滾、並發。
3. 完整診斷與網路故障、擴展相容矩陣。
4. 獨立離線企業包、名稱掃描、manifest／簽名／來源與必要通知。
5. 乾淨普通帳戶與目標端點試運行、完整逐項審核，全部有證據後才放行。
