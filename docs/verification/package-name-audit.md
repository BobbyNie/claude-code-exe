# 交付名稱掃描器（A02）

此工具只建立名稱檢查證據，不批准發布、不修改檔案，也不驗證二進位來源、
簽名、授權、功能或普通帳戶相容性。正式交付包尚未完成全量驗收。

## 政策與邊界

- `--restricted-name` 必須由交付負責人逐一提供；大小寫不敏感。示例名稱不是
  已核准的企業禁用清單，沒有提供清單時不得默認通過。
- 解包目錄遞迴檢查所有目錄／檔名，包括舊版及更新殘留；使用者選擇的掃描
  根目錄及其父目錄名稱不在掃描範圍。ZIP 本身的交付檔名及成員名稱亦檢查。
- 除明確列出的原始二進位外，所有普通檔案按 UTF-8（可有 BOM）或帶 BOM 的
  UTF-16 檢查。JSON 同時檢查解碼後的公開鍵和值，避免 Unicode escape 漏檢。
  無法解碼的內容、含 NUL 或損壞 JSON 不會被當作通過。
- `--opaque` 只允許存在的、精確相對路徑的 `.exe`／`.dll`，且有 MZ 標頭；
  這不是完整 PE／簽名驗證。其內部字串不掃描、不修改，排除清單寫入報告。
  **檔名不豁免，公開設定及必要通知不能用此參數豁免。**
- 必要通知若含禁用名稱，結果是衝突／未通過；不能刪除或改寫通知以通過。
- 不追蹤目錄中的 symlink／Windows reparse point。ZIP 不解壓執行，拒絕連結、
  絕對／跳出路徑、反斜線／冒號路徑及大小寫碰撞。掃描輸出不包含匹配的內容。
- 當前工具不解釋任意其他設定語言的 escape、不掃描 ZIP 註解、PE metadata 或
  簽章發行者。需要把這些納入時，必須擴展明確範圍及測試，不可宣稱全面無原名。
- 請對停止寫入、已固定的候選包操作。此報告不是簽名 manifest，也沒有宣稱
  具備抵抗並發修改／TOCTOU 的安全隔離。

## 使用

以下路徑及禁止名稱均是操作示例，須換成實際交付候選及核准清單。報告存放在
候選之外，避免掃描報告自身的禁止名称字串；禁止修改正式用戶資料來準備交付包。

```powershell
python C:\source\scripts\ccode\package_audit.py C:\candidates\ccode `
  --restricted-name restricted-example --opaque ccode.exe > C:\evidence\unpacked-names.json
if ($LASTEXITCODE -ne 0) { throw 'Unpacked delivery name audit failed' }

python C:\source\scripts\ccode\package_audit.py C:\candidates\ccode.zip --archive `
  --restricted-name restricted-example --opaque ccode.exe > C:\evidence\archive-names.json
if ($LASTEXITCODE -ne 0) { throw 'Archive delivery name audit failed' }
```

退出碼：`0` 是聲明範圍的名稱掃描通過；`1` 有發現；`2` 政策／輸入／ZIP 讀取
錯誤。非零均阻止該項放行。兩種輸入都要實際跑，不能拿單元測試或只掃描壓縮前
目錄代替交付 ZIP 驗收。還要將報告關聯至正式包 hash、版本、平台及帳戶等全量
驗收 metadata，並另行完成來源、通知、再分發批准及可信簽名門檻。
