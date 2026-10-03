# API transport 修復提案（尚未批准或實作）

日期：2026-10-03。目的：解決 unified sandbox Release 的真實 DNS／TLS 阻塞，並非更改失敗標準。

## 已核實的起點

- 最新流程 37025702587、程式提交 39b61ac930d6eb3bfe4c294ed6f761bd88835ad1 仍為 completed/failure。
- 2.1.221：DNS、過期證書、不受信任證書失敗；2.1.282：DNS、過期證書失敗。
- checksum-verified 2.1.282 原始負載的 typed API error adapter 不保留 DNS／expiry cause。
  `frontend.hpp` 能解析合成 structured error，不代表真實引擎會產生該事件。
- 不採用 arbitrary message matching、debug log 落盤、改寫官方負載、關閉 TLS 或預檢取代真實請求。

## 選項及取捨

1. 等待上游輸出可靠的 structured cause：改動少，但目前沒有可驗證的可用修復，亦不能解決旧版相容驗收。
2. 增加前端原生 API transport（建議，需確認架構變更）：實際 engine HTTP 請求經每次 invocation 的 loopback bridge，
   bridge 使用 Windows 原生網路介面連到設定的上游，從該次真實連線取得 DNS／憑證錯誤。
   這增加 HTTP／streaming／取消／憑證信任處理的責任，必須完整測試，不是小型錯誤字串補丁。
3. 只做前置連線檢查：可能改善提示，但不能證明實際請求採用相同解析、連線及憑證；不作為修復選項。

## 建議方案的不可變條件

- 官方 engine bytes 不變；不新增服務、驅動、管理員安裝或外部 runtime 依賴。
- 不是透明攔截系統流量，只轉接明確設定的模型 API；不宣稱這形成 OS 安全沙箱。
- 只監聽 loopback，使用 invocation 專屬不可猜測 capability；上游固定，不接受任意代理目的地。
- TLS 必須驗證主機、有效期及信任鏈；不使用忽略憑證錯誤旗標；不自動安裝信任根。
  Windows trust store 與既有 NODE_EXTRA_CA_CERTS 的差異必須明確處理，不能靜默遺失企業 CA 支援。
- 結構化錯誤只能來自本次實際 upstream request；無額外探測請求，不重試／重放有副作用請求。
- 保持 status、API 路徑、必要 headers、SSE 串流、取消與 timeout 行為；禁止自動跨來源 redirect 搬移憑證。
- 憑證、prompt、body、原始錯誤只允許必要的記憶體處理，不寫入 CI／診斷檔；對外只輸出中性代碼。
- bridge、engine 與子程序具明確生命周期；失敗、取消、退出必須回收 listener、連線、threads 和 handles。

## TDD 與驗收順序

1. 真實 loopback request 驅動 DNS failure，驗證可信錯誤分類、零重試及隱私，不使用 preflight 作證據。
2. 不受信任／過期／主機名不符證書逐一 RED→GREEN；有效 CA 及有效證书成功是必須對照，避免全部拒絕的假綠燈。
3. 真實 SSE、多輪工具、429、取消、斷流、redirect、並行及大小上限回歸；兩版本 engine 都接入 bridge。
4. Windows hosted 全矩陣通過後，核實來源清單、企業 package／签名及剩餘資料安全門檻，再產出綁定 SHA 的獨立 Release。

本文件僅記錄提案，不聲稱 transport 已實作、不移除任何現有失敗 gate，亦不聲稱 Release 已完成。
