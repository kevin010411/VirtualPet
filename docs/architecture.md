# 韌體架構與分階段精簡方案

本文件描述韌體的責任分工，以及接下來的架構清理順序。2026-10-05 工作區的專案 environment 為 `project_29`；下方 `project_12` 數字與階段驗證屬歷史紀錄。Pet Stat、Action、Status、Evolution、持久化與 SD 失敗語意，以 `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md` 為準；此處只記韌體實作安排。

## 目前的執行路徑

`src/platform/main.cpp` 建立板級裝置、SD、Renderer、Pet、PetStorage、SdAppearanceLoader 與 Game，處理電源、按鍵及主迴圈。`Game` 協調啟動、流程、行為、動畫、命令、外觀、渲染與存檔。各功能的主要責任如下：

| 模組 | 目前責任 |
| --- | --- |
| `pet/`、`pet_behavior/` | Pet 狀態、Pet Stat/Action 規則、執行期行為及存檔；`RuntimeContractLoader` 與 `RuntimeTableBehavior` 目前也承擔 SD 表格載入 |
| `appearance/` | 外觀選擇流程、Evolution/Outfit 查詢及 SD adapter |
| `animation/`、`commands/`、`minigames/` | 動畫排程、System Command、猜物小遊戲 |
| `presentation/` | Game 流程、版面、Renderer 與逐幀解碼 |
| `shared/assets/` | `/runtime.bin` 的共用資料結構與 `.data` pack 存取 |
| `shared/runtime_table/` | Runtime Table envelope、段落與記錄讀取、數字動畫參照解析，以及 SD snapshot 生命週期 |
| `platform/` | STM32、TFT、SD、按鍵與電源整合 |

正式韌體只讀目前的 `/runtime.bin` v9。啟動時先讀 manifest，再由同一次完整表讀取取得初始外觀及其設定；Game 保存已驗證的初始外觀。恢復不同外觀、重置及日後切換外觀仍走既定的重新載入流程。錯誤保留第一個資源名稱，失敗後需重新開機。這些行為不能因移動檔案或收斂介面而改變。

## Proposed Architecture：以責任收斂取代層數增加

1. **平台組裝。** `platform/main.cpp` 只持有裝置生命週期、輸入與電源流程，並組裝一次性物件；不在平台層解碼 runtime 規則。
2. **啟動契約載入。** 一個清楚的載入介面交付已驗證的 manifest、初始外觀與對應設定。對外只暴露成功結果與第一個錯誤資源；開檔、envelope、必要段落及 bounded read 留在實作內。一般外觀切換保有重新驗證路徑。
3. **執行期規則。** Pet Behavior、Status、Evolution 和 Action 的規則使用已解碼的固定容量資料，不自行開 SD。只保留被現行匯出契約與功能實際需要的查詢。
4. **外觀操作。** 外觀選單負責互動狀態，載入模組負責 SD 查詢、預覽與錯誤。逐一檢查 `AppearanceLoader` 的方法：若只有單一 adapter 的轉呼叫，且沒有替呼叫端隱藏驗證、錯誤或生命週期複雜度，就合併到真正擁有該行為的模組；不為了目錄對稱新增 port。
5. **渲染與資源。** Renderer 管理畫面與播放，BundleReader 管理 pack 格式及讀取，FrameDecoder 管理像素解碼。逐幀路徑不加入為節省 Flash 而增加的迴圈、開檔或轉接呼叫。

這是目標責任，不表示現有檔案已完全符合。2026-10-05 已將外觀解碼與查詢移至 `appearance/domain/RuntimeTableAppearance.cpp`；共用二進位讀取與 SD snapshot 分別由 `RuntimeTableReader`、`RuntimeTableFile` 實作。`detail/` 介面僅供解碼器共享，呼叫端仍使用原有載入操作。行為、畫面配置解碼與完整載入順序仍在 `RuntimeTableBehavior.cpp`，後續可再依責任收斂。調整時先確認呼叫者、失敗順序與資料持有者，避免另疊一層 façade。

## 逐步清理順序

| 步驟 | 清理內容 | 完成條件 |
| --- | --- | --- |
| 1. 文件與建置真相 | 移除指向已不存在 environment、固定照護值及舊 Status mode 的現行指引 | 文件指向 `platformio.ini` 的 `project_12` 和現行 `/runtime.bin`；歷史資料不再被當成使用說明 |
| 2. 載入責任 | 盤點 Runtime Table 及外觀查詢的重複開檔、manifest 驗證和錯誤轉譯；選一條流程先收斂 | 對外介面變小，第一錯誤資源、bounded read、恢復存檔與外觀切換語意保持；host 測試及 `project_12` 建置通過 |
| 3. 過薄介面 | 逐個審核只轉呼叫的 adapter/port 與未再使用的相容入口 | 證明現行呼叫者及 Profile Resolver 輸出不再依賴後才移除；不刪除目前使用的第三方庫 |
| 4. Game 協調 | 在載入責任穩定後，移除 Game 中重複的狀態與流程判斷 | 啟動、存檔恢復、初次啟動、重置與 fatal 流程有可維護的驗證；不增加 SRAM 峰值 |
| 5. 熱路徑與容量 | 單獨評估逐幀 pack 存取、Renderer 緩衝及配置 | 有實機時間/記憶體量測才宣稱效能收益；Flash/RAM 以同一設定的 A/B 連結結果報告 |

每一步只保留有實際責任收益的改動。Status 等級計算保留除法，不以減法迴圈換取 Flash；不採用僅調整字串組裝或內聯標註的局部省位元組方案。第三方圖形與 SdFat 依賴保持可用。

## 第四步後的下一個結構規劃

2026-10-05 已將 `Game` 的啟動準備、啟動完成、存檔恢復與重置集中於 `GameStartup.cpp`，仍是同一個 Game 的實作，不增加 controller 或公開介面。`PreparationState` 取代 `setupPrepared`、`initialStateLoadingFailed` 與 `startupConfigError`，統一表示未準備、已準備、契約失敗或 Pet 狀態失敗。`runtimeLoadState` 表示目前契約是否有效，`initialized` 表示可否執行遊戲迴圈；兩者與準備結果用途不同，保留分工。一般輸入、播放與 Evolution 協調仍在 `Game.cpp`。

同日進一步精簡 `PetActionController`：Pet 狀態讀取與原子提交由 `PetBehaviorRuntime` 直接使用 `Pet`，Status 命令以 `const Pet` 取得 snapshot；Game 的狀態欄位操作也直接使用既有 Pet。Controller 保留存檔節奏、Evolution 查詢結果篩選及外觀套用／顯示／存檔協調共六個操作；移除純轉呼叫與無使用者的 load/reset/Evolution 套用入口。初始化與重置仍由 `GameStartup.cpp` 協調，不新增替代生命週期。

第四步以 `Game::prepare_game`、恢復、重置及 fatal 的 host 整合測試作為流程回歸入口。第五步先保持現有模組分工：`AnimationController` 決定播放狀態，`Renderer` 持有顯示與緩衝，`BundleReader` 驗證及定位 pack frame，`FrameDecoder` 串流解碼並送往 TFT。不要先增加一層播放 façade，或把 SD 開檔責任搬進 Game。

| 順序 | 先確認的事 | 可評估的結構變更 | 採納條件 |
| --- | --- | --- | --- |
| 1. 靜態責任收斂 | 無法建立實機逐幀基線時，先追查外觀查詢的結果、首錯資源與責任歸屬 | Evolution 查詢已區分找到、無目標、載入失敗；完整契約與其餘外觀查詢共用內部檔案生命週期與錯誤記錄 | 保留目前重新驗證與失敗語意；host 測試及 `project_12` 建置通過，不宣稱速度收益 |
| 2. Pack 存取 | `openFrame` 每幀開檔並驗證 header，已快取上一個 animation record；確認重複成本、切換 species/outfit、缺檔和首錯資源 | 若開檔確為瓶頸，再評估由 `BundleReader` 持有單一目前 pack 的開檔生命週期與失效規則 | 正常/切換/故障路徑測試通過；實機逐幀時間有收益，Flash/RAM 在容量內 |
| 3. Renderer 緩衝 | 目前 1,024 B 讀緩衝與 128×12 RGB565 行緩衝由 `AnimationState` 一次配置 | 比較 12 行與較小批次，或調整固定生命週期配置；不改變 `FrameDecoder` 的有界讀取與錯誤拒絕 | 以 SRAM 峰值及 SPI/解碼時間 A/B 決定，不只看靜態 RAM 或 Flash |

目前無法建立實機 SD 時間與記憶體基線，已先以靜態分析收斂 Evolution 查詢結果、完整契約與外觀查詢共用的檔案生命週期，以及外觀查詢的錯誤記錄。每次查詢仍開檔並驗證，逐幀熱路徑維持現狀，不宣稱效能改善。Pack 常駐開檔與 Renderer 緩衝調整仍須實機量測才決定；目前沒有證據支持新增常駐快取或擴大對外介面。

## 驗證與回退

畫面版面目前以 Runtime Table v9 的必要 SCREEN_BLOCKS／SCREEN_RULES 載入，
`.data` v2 Layout 產品包含完整背景、積木矩形及數值圖案。後端預合成素材，
韌體只做有界載入與串流繪製，Command Layout 槽與圖片產品身份分開。
須整包重新匯出並配對本版韌體；舊 runtime 或混合 bundle 不做猜測回退。
2026-10-02 的 `project_12` linked Flash 為 59,864／65,536 bytes，靜態 RAM
7,916／20,480 bytes；畫面、數值、reader、playback、startup 及 persistence
hosts 已通過。Web 正式客製入口仍受完整驗證 gate 保護，實機 SD／TFT／按鍵
未驗收，不能把 host 結果或靜態 RAM 當作設備表現。逐票證據見 Web
`.scratch/screen-layout-runtime/verification.md`。

目前使用者的 `platformio.ini` 為 `project_29`／`genericSTM32`。依本次選擇，尺寸比較使用獨立 `.pio/action-refactor.ini`，指定 `genericSTM32F103C8` 並保留相同 profile flags；建置命令為 `platformio run -c .pio/action-refactor.ini -e project_29`，不修改使用者的設定。結構變更需執行受影響的 host 測試；更動 profile flag 時，再核對 Web Profile Resolver 實際產生的設定。每次記錄修改前後的 linked Flash 與靜態 RAM。沒有實機 SD/時間量測時，只能報告靜態流程、建置與測試結果。

實作進度、A/B 數字與未驗證項目記於 [`.agent/firmware-refactor-progress.md`](../.agent/firmware-refactor-progress.md)。
