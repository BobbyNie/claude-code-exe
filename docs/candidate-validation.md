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
3. 從指定會話讀取目前工作區的第一則使用者純文字訊息；不存在、歧義、
   混合非文字內容或超限時拒絕，不猜測或修改歷史。
4. 在候選內用實際引擎恢復該 session，禁止內建工具，要求回覆首則歷史文字。
   回覆只在記憶體比對，不輸出到前端。必須成功完成、session ID 不變且文字完全一致。
5. 再核對來源快照；對引擎恢復後的候選建立獨立 `verified/UUID` 快照。
6. 寫入候選的 `validation.json`，記錄引擎版本及 SHA-256、adapter、workspace UUID、
   session ID、來源及驗證快照 UUID、驗證後檔案雜湊清單。原 `candidate.json` 仍為 staged。

回執的 `scope` 固定為 `single-session`。它是本機一致性證據，**不是數位簽章或
對同帳戶惡意程式的安全保證**。讀取端須重新核對引擎、驗證快照及候選目前檔案；
不能僅信任 `historyVerified: true`。目錄／ACL 等完整語義仍須另行驗收。

## 失敗及限制

- 錯誤答案、引擎錯誤、取消或資料變更不產生成功回執；不自動重放 API 請求。
- 引擎嘗試恢復後可能已改變候選，即使驗證失敗。保留候選供診斷，重新從原快照
  stage 新候選再試，不覆蓋舊資料、不盲目合併 JSONL。
- 成功回執不得覆寫。候選後續正常使用造成變動後，先前回執不再證明目前檔案。
- 此命令目前只驗證所選會話；其他會話、搬移、版本回退、原子啟用、磁碟滿及
  中斷恢復不因本命令成功而視為通過。
