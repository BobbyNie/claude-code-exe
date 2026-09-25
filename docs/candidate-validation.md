# 隔離候選驗證（尚不啟用）

此流程屬 A 方案的資料遷移準備，不提供 OS 安全沙箱。它不切換目前 profile，
也不代表完整遷移、所有歷史或企業端點已驗收。

## 命令

在原工作區執行，沿用相同的 `--data-dir`：

```text
ccode.exe --data-dir DATA --snapshot-profile
ccode.exe --data-dir DATA --stage-profile SNAPSHOT_ID
ccode.exe --data-dir DATA --validate-profile CANDIDATE_ID --resume SESSION_ID
```

前兩個命令分別輸出快照及候選 UUID。第三個命令需要核准 API 的
`A_BASE_URL` 與 `A_AUTH_TOKEN`／`A_API_KEY`；可另帶 `--model NAME`。
驗證會向已設定 API 提交一次恢復會話請求，可能產生正常 API 費用。
歷史仍按正常恢復流程傳給該 API；預期答案不會加進新提示。

## 驗證內容與資料

1. 同時持有原 profile 及候選 profile 的排他鎖。
2. 檢查來源快照、候選初始清單及 SHA-256，拒絕已變更或非 staged 的候選。
   原 profile 的非操作鎖檔案必須仍與快照完全相符；不符時以 `E_SOURCE_CHANGED`
   在模型請求前拒絕，保留所有資料，應重新建立快照及候選，不自動合併。
3. 從指定會話讀取目前工作區的第一則使用者純文字訊息；不存在、歧義、
   混合非文字內容或超限時拒絕，不猜測或修改歷史。
4. 在候選內用實際引擎恢復該 session，禁止內建工具，要求回覆首則歷史文字。
   回覆只在記憶體比對，不輸出到前端。必須成功完成、session ID 不變且文字完全一致。
5. 再核對來源快照及原 profile，拒絕驗證期間的來源變更；對引擎恢復後的候選建立獨立 `verified/UUID` 快照。
6. 寫入候選的 `validation.json`，記錄引擎版本及 SHA-256、adapter、workspace UUID、
   session ID、來源及驗證快照 UUID、驗證後檔案雜湊清單。原 `candidate.json` 仍為 staged。

指定 `--resume` 時回執的 `scope` 為 `single-session`。它是本機一致性證據，**不是數位簽章或
對同帳戶惡意程式的安全保證**。讀取端須重新核對引擎、驗證快照及候選目前檔案；
不能僅信任 `historyVerified: true`。目錄／ACL 等完整語義仍須另行驗收。

## 失敗及限制

- 錯誤答案、引擎錯誤、取消或資料變更不產生成功回執；不自動重放 API 請求。
- 引擎嘗試恢復後可能已改變候選，即使驗證失敗。保留候選供診斷，重新從原快照
  stage 新候選再試，不覆蓋舊資料、不盲目合併 JSONL。
- 成功回執不得覆寫。候選後續正常使用造成變動後，先前回執不再證明目前檔案。
- 單一會話模式只驗證所選會話；其他會話、搬移、版本回退、原子啟用、磁碟滿及
  中斷恢復不因本命令成功而視為通過。

來源檢查只證明本次驗證檢查點的檔案一致性。後續啟用仍必須持鎖重新核對，
不能用本回執跳過啟用時的來源衝突檢查；同帳戶外部程式仍可能不遵守操作鎖。

## 所有頂層會話模式

```text
ccode.exe --data-dir DATA --validate-profile CANDIDATE_ID --all-sessions
```

`--all-sessions` 與 `--resume` 互斥，而且只能與 `--validate-profile` 一起使用。
仍可指定 `--model`。此模式會對每個頂層會話提交一次恢復請求，API 成本隨會話數增加。

- 從所有原生 project 目錄的頂層 JSONL 盤點，不依賴可重建索引或目前 cwd。
- 先預檢全部檔案，拒絕重複 session ID、損壞 JSON／末行、未知會話檔名、
  無主會話 user 記錄、非純文字首訊息；沒有可驗證會話也拒絕，不把空集合當通過。
- 每個會話的絕對 cwd 必須仍存在；不存在回傳 `E_CANDIDATE_WORKSPACE`。
  不猜測搬移映射、不修改歷史路徑；明確遷移流程仍另行實作。
- 逐一以該會話原 cwd 啟動實際引擎，不改前端程序的全域 cwd。任何一次失敗停止，
  不重放、不發布部分成功回執。候選可能已被前幾次恢復修改，應重新 stage 再試。
- 初始歷史標記保留在記憶體，後續 probe 不能把已替換的標記當新預期答案。
  結束後再核對會話集合／工作區／標記，才建立整體凍結快照及回執。
- 回執 `scope` 為 `all-top-level-sessions`，`sessions` 陣列記錄每個 session UUID
  與 workspace UUID。讀取端重新盤點並核對完整清單，拒絕遺漏／重複項目。

此範圍不是「所有功能相容」：巢狀子代理資料會隨 profile 保留，但不宣稱已獨立
恢復每個子代理；技能／MCP、目錄 ACL、搬移、版本回退及企業端點仍須各自驗收。
候選仍為 staged，這個命令不切換 active profile。

## 啟用已驗證候選（Windows 回歸待驗）

`ccode.exe --data-dir DATA --activate-profile CANDIDATE_ID` 僅接受全頂層會話驗證
回執，不與會話／模型／快照／驗證操作混用，不需要 API 請求。啟用時在資料根、
來源及候選的獨占鎖內重新核對來源快照、候選回執、引擎版本與 SHA-256。
來源已變更則 `E_SOURCE_CHANGED`，必須從新來源重新備份及驗證，不自動合併。

指標經同目錄 `active-profile.json.pending` 寫入及 flush 後替換正式指標。
既有 pending 會使本次啟用停止，不覆寫中斷證據，也不自動重放。
可執行 `ccode.exe --data-dir DATA --archive-activation-pending`，在資料根獨占鎖內
將原始 bytes 移至 `activation-recovery/UUID/pending.json` 並輸出 UUID。即使 pending
截斷或正式指標損壞也不解析／改寫它們，不建立 fallback profile，不需要 API。
無 pending、非法檔案或歸檔碰撞明確拒絕；失敗保留證據，不承諾斷電耐久性。
啟用建立 pending 時，已有同名檔案回報 `E_ACTIVATION_PENDING`；其他建立、
寫入、flush、close 或替換失敗回報 `E_ACTIVATION_WRITE`，不自動重試。
此命令不修復損壞的正式指標、不啟用候選；之後重試啟用仍須通過完整驗證。
原 profile、來源快照與 verified 凍結副本都保留，後續會話寫入所選
候選的 live profile。指標是唯一啟用提交記錄，candidate metadata 保持 staged。

這不表示已實作跨引擎回滾、斷電耐久保證或細粒度並發。不要手工編寫指標來繞過
啟用驗證；引擎不相容時停止，不自動退回其他資料。完整回滾及故障驗收仍待完成。

## 引擎不符時的資料保全

`--snapshot-profile` 是獨占的備份命令，不能混用會話、模型、候選或提示參數。
即使目前程式的引擎與 active 指標記錄的引擎不同，也可在資料根及 profile 鎖內
建立 SHA-256 核驗快照，不啟動引擎、不需 API、不改寫歷史。指標與回執的引擎、
候選、驗證身份仍必須一致；損壞或缺失的 active 目標不會退回舊 profile。

這是回退前保留現有資料的操作，不是相容性認證。一般會話、候選驗證及啟用仍
拒絕引擎不符。尚未提供完整跨引擎回退命令，也不能把備份成功視為舊引擎可讀
新版資料；相容快照選擇、目標引擎實際驗證和回退提交仍需完成。

## 回退準備（不是回退完成）

```powershell
ccode.exe --data-dir DATA --prepare-rollback SNAPSHOT_ID
```

使用預計回退到的程式版本執行，明確指定先前保留的快照 UUID。此命令不需要
API 憑證、不啟動引擎，不能混用其他操作、模型選項或提示。即使目前 active
記錄的是另一個引擎，只要指標及回執身份一致，仍可先保全資料。

命令先核驗來源，完整備份目前 active，再把指定舊快照複製到
`DATA/rollback-candidates/UUID`。stdout 回傳 JSON 準備計畫，包括 candidateId、
sourceSnapshotId、preservationSnapshotId、priorActivePointer、targetEngine 及 adapter；
同份計畫保存在候選的 rollback.json。正式指標與現有歷史不變，不盲目合併。

**prepared 不代表相容，也不會啟用候選。** 必須先完成全會話驗證，再明確提交；不可把此目錄搬入普通 candidates 或手動修改 active 指標以繞過驗證。
若中途失敗，保留已產生的備份／候選供核對，不自動清理或重放。


## 回退候選全會話驗證

```powershell
ccode.exe --data-dir DATA --validate-rollback CANDIDATE_ID
```

使用與準備計畫 targetEngine 完全一致的程式，配置核准 API 憑證與 gateway。
命令固定驗證全部頂層會話；只能額外指定 `--model`，不能用 `--resume`、
`--all-sessions`、工具選項或提示縮小／改寫驗證。驗證在候選中執行，停用工具，
依每個會話的原工作區恢復並要求回覆既有歷史標記，不把預期答案放入提示。

每次探測前後核對目前 active、保全快照、來源快照、原指標與準備計畫。
變動或恢復失敗即停止；不自動重試。成功後寫出綁定計畫的
`rollback-validation.json`，保留 `rollback-verified` 凍結副本。已有回執或
pending 證據時拒絕重跑；失敗後保留證據，不手動刪除檔案以繞過檢查。

此命令仍不切換 active。實際模型與企業 gateway
相容性仍須以目標環境驗收，CI 的本機假 API 不能替代。


## 明確提交已驗證回退

```powershell
ccode.exe --data-dir DATA --activate-rollback CANDIDATE_ID
```

只接受準備計畫指定的目標引擎及完整驗證回執，不需要 API 憑證，不能混用其他
操作或引擎參數。持有資料根、目前 active 及候選鎖後，再核對計畫、來源／保全
快照、候選與凍結內容；目前 active 或指標有新變動時拒絕提交，不丟棄較新資料。
成功後原子切換 schema 2 回退指標，保留舊 active 與保全快照；重複啟用拒絕。
若存在 pending 證據，先核對再使用既有明確歸檔命令，不手動覆寫或自動重放。

公開提交的 Windows 端到端結果待 CI；這不代表真實跨版本回退或企業驗收完成。
