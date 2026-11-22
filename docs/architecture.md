# 韌體架構

`platform/main.cpp` 組裝板級裝置、SD、Renderer、Pet／PetStorage 與 Game。
主迴圈處理電源、按鍵、RTC 恢復與 Game 更新。`Game` 協調流程，不承擔 Web 的產品驗證。

| 目錄 | 責任 |
| --- | --- |
| `platform/` | STM32、板級腳位、RTC、按鍵、TFT／SD 接入與電源；音訊 PWM／DMA driver |
| `controller/` | Game 啟動與流程、命令選擇／派送、Status 與小遊戲接入 |
| `pet/` | Pet 狀態、Session 新建／恢復／重置、Action 規則、日推進與保存 |
| `appearance/` | 外觀選單、Evolution 生命週期、外觀變更與 SD 查詢 |
| `collection/` | 圖鑑 UI、條件與取得判定；受圖鑑旗標控制 |
| `game/` | 小遊戲輸入、回合與勝負規則；抓取的素材讀取與場景合成 |
| `shop/` | 商店資料讀取、交易與介面繪製 |
| `animation/` | 固定容量播放序列、基礎動畫與序列音訊協調 |
| `audio/` | 音訊播放器、緩衝與執行錯誤 |
| `display/` | LayoutRenderer、Renderer、逐幀解碼與 TFT 顯示 |
| `resources/` | Runtime Contract、Table 有界解碼、SD snapshot 與 `.data` pack 存取 |
| `common/` | Profile、CRC、隨機数、文字及 SD 共用工具 |

新檔案放在擁有責任的目錄，`include/` 與 `src/` 對應；共用 decoder 內部介面放 `detail/`。
第三方庫由 PlatformIO 管理，不為目錄對稱增加 adapter／port。

## 主要路徑

- `GameStartup → PetSession → RuntimeContractLoader` 負責啟動，載入 `/runtime.bin` 標頭、初始外觀及執行契約。
  存檔相容時恢復遊戲，否則建立新局。Game 啟用顯示、命令與播放。
- 命令：`CommandController → CommandExecutor → PetBehaviorRuntime` 或 System Command。
  Action 先原子提交數值與暫停，再提交完整播放序列。
- 換日：RTC／SessionClock／PetClock 提供日邊界，Game 執行一次日交易；
  預設啟用的每日後處理協調解鎖、保存與 Evolution。
- 外觀：`AppearanceChangeController` 負責套用與保存；`EvolutionController`
  負責進化播放與轉換時機，手動選單由 `AppearanceSelectionController` 處理。
- 畫面：`LayoutRenderer` 同步版面，`AnimationController` 選影格，
  `BundleReader` 定位 payload，`FrameDecoder` 串流送 TFT。

## 前景畫面

`AppFlowController` 保存目前階段，以及低電量和進化結束後返回的階段。
`GameForeground.cpp` 依階段交付按鍵、重繪及取消。商店、圖鑑、外觀選擇與小遊戲各自保存當前選項和進度。
Fatal 取消暫態工作並停用互動。低電量的進入與返回由 Game 協調。

一般重繪與 STOP 喚醒共用畫面分流。抓取畫面與事件頁先處理，靜態選單、商店及圖鑑接著依目前狀態繪製。
進行中的商店或圖鑑動畫由播放控制器繪製。喚醒會強制重畫一般動畫，外觀選擇器直接重畫當前選項以免恢復背景音樂。

選單、商店、圖鑑和抓取持續暫停寵物時鐘。一般指令動畫、事件結算和指令反應依各自的播放與輸入狀態協調時鐘。
方向鍵可以在補算日數時瀏覽選單，會變更寵物狀態的確認操作仍遵守補算順序。

## 設定與場景

`RuntimeContractLoader` 在同一次載入填入 `RuntimeContract` 內各功能的設定。
寵物行為、按鈕綁定、版面、狀態畫面、動畫音訊、圖鑑及功能設定各有對應的讀取介面。
Game 持有設定，功能借用其中需要的資料。每次外觀套用完成後再重新配置消費模組。

`ShopData` 解析商店檔案並按需讀取記錄，`ShopSession` 管理操作、交易與保存，`ShopView` 負責畫面和局部更新。
`CaptureGame` 管理計時、碰撞與捕獲，`CaptureScene` 合成有變動的像素帶和圖卡。
場景借用 Renderer 的像素緩衝，抓取像素帶沿用同步 DMA 傳輸。

Web 規格管理匯出格式。韌體的 SD 路徑與格式版本見[SD 資源與渲染](renderer_asset_formats.md)。
資料／素材完整驗證在 exporter。韌體保留必要讀取、容量與硬體失敗處理，
記錄第一個資源錯誤。Fatal 關閉互動、取消暫態工作，重開機才恢復。
不要把歷史重構計畫或當時的 Flash 數字當成尚待執行工作。
