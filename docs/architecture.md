# 韌體架構與分階段精簡方案

本文件描述目前 `project_12` 韌體的責任分工，以及接下來的架構清理順序。Pet Stat、Action、Status、Evolution、持久化與 SD 失敗語意，以 `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md` 為準；此處只記韌體實作安排。

## 目前的執行路徑

`src/platform/main.cpp` 建立板級裝置、SD、Renderer、Pet、PetStorage、SdAppearanceLoader 與 Game，處理電源、按鍵及主迴圈。`Game` 協調啟動、流程、行為、動畫、命令、外觀、渲染與存檔。各功能的主要責任如下：

| 模組 | 目前責任 |
| --- | --- |
| `pet/`、`pet_behavior/` | Pet 狀態、Pet Stat/Action 規則、執行期行為及存檔；`RuntimeContractLoader` 與 `RuntimeTableBehavior` 目前也承擔 SD 表格載入 |
| `appearance/` | 外觀選擇流程、Evolution/Outfit 查詢及 SD adapter |
| `animation/`、`commands/`、`minigames/` | 動畫排程、System Command、猜物小遊戲 |
| `presentation/` | Game 流程、版面、Renderer 與逐幀解碼 |
| `shared/assets/` | `/runtime.bin` 的共用資料結構與 `.data` pack 存取 |
| `platform/` | STM32、TFT、SD、按鍵與電源整合 |

正式韌體只讀 `/runtime.bin` v1。啟動時先讀 manifest，再由同一次完整表讀取取得初始外觀及其設定；Game 保存已驗證的初始外觀，供新狀態與工作階段內重置使用。恢復不同外觀及日後切換外觀仍重新讀取並驗證。錯誤保留第一個資源名稱，失敗後需重新開機。這些行為不能因移動檔案或收斂介面而改變。

## Proposed Architecture：以責任收斂取代層數增加

1. **平台組裝。** `platform/main.cpp` 只持有裝置生命週期、輸入與電源流程，並組裝一次性物件；不在平台層解碼 runtime 規則。
2. **啟動契約載入。** 一個清楚的載入介面交付已驗證的 manifest、初始外觀與對應設定。對外只暴露成功結果與第一個錯誤資源；開檔、envelope、必要段落及 bounded read 留在實作內。一般外觀切換保有重新驗證路徑。
3. **執行期規則。** Pet Behavior、Status、Evolution 和 Action 的規則使用已解碼的固定容量資料，不自行開 SD。只保留被現行匯出契約與功能實際需要的查詢。
4. **外觀操作。** 外觀選單負責互動狀態，載入模組負責 SD 查詢、預覽與錯誤。逐一檢查 `AppearanceLoader` 的方法：若只有單一 adapter 的轉呼叫，且沒有替呼叫端隱藏驗證、錯誤或生命週期複雜度，就合併到真正擁有該行為的模組；不為了目錄對稱新增 port。
5. **渲染與資源。** Renderer 管理畫面與播放，BundleReader 管理 pack 格式及讀取，FrameDecoder 管理像素解碼。逐幀路徑不加入為節省 Flash 而增加的迴圈、開檔或轉接呼叫。

這是目標責任，不表示現有檔案已符合。`pet_behavior/domain/RuntimeTableBehavior.cpp` 目前仍讀 SD；`Game` 仍協調多個載入結果；`AppearanceLoader` 仍有多個查詢方法。調整時先確認呼叫者、失敗順序與資料持有者，再以替換舊路徑的方式收斂，避免另疊一層 façade。

## 逐步清理順序

| 步驟 | 清理內容 | 完成條件 |
| --- | --- | --- |
| 1. 文件與建置真相 | 移除指向已不存在 environment、固定照護值及舊 Status mode 的現行指引 | 文件指向 `platformio.ini` 的 `project_12` 和現行 `/runtime.bin`；歷史資料不再被當成使用說明 |
| 2. 載入責任 | 盤點 Runtime Table 及外觀查詢的重複開檔、manifest 驗證和錯誤轉譯；選一條流程先收斂 | 對外介面變小，第一錯誤資源、bounded read、恢復存檔與外觀切換語意保持；host 測試及 `project_12` 建置通過 |
| 3. 過薄介面 | 逐個審核只轉呼叫的 adapter/port 與未再使用的相容入口 | 證明現行呼叫者及 Profile Resolver 輸出不再依賴後才移除；不刪除目前使用的第三方庫 |
| 4. Game 協調 | 在載入責任穩定後，移除 Game 中重複的狀態與流程判斷 | 啟動、存檔恢復、初次啟動、重置與 fatal 流程有可維護的驗證；不增加 SRAM 峰值 |
| 5. 熱路徑與容量 | 單獨評估逐幀 pack 存取、Renderer 緩衝及配置 | 有實機時間/記憶體量測才宣稱效能收益；Flash/RAM 以同一設定的 A/B 連結結果報告 |

每一步只保留有實際責任收益的改動。Status 等級計算保留除法，不以減法迴圈換取 Flash；不採用僅調整字串組裝或內聯標註的局部省位元組方案。第三方圖形與 SdFat 依賴保持可用。

## 驗證與回退

目前 `platformio.ini` 的預設及唯一專案 environment 是 `project_12`。結構變更至少建置 `platformio run -e project_12`，並執行受影響的 host 測試；更動 profile flag 時，再核對 Web Profile Resolver 實際產生的設定。每次記錄修改前後的 linked Flash 與靜態 RAM。沒有實機 SD/時間量測時，只能報告靜態流程、建置與測試結果。

實作進度、A/B 數字與未驗證項目記於 [`.agent/firmware-refactor-progress.md`](../.agent/firmware-refactor-progress.md)。
