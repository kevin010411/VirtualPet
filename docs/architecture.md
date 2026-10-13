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
| `game/` | GuessItemGame 的輸入、回合與勝負規則 |
| `animation/` | 固定容量播放序列、基礎動畫與序列音訊協調 |
| `audio/` | 音訊播放器、緩衝與執行錯誤 |
| `display/` | LayoutRenderer、Renderer、逐幀解碼與 TFT 顯示 |
| `resources/` | Runtime Contract、Table 有界解碼、SD snapshot 與 `.data` pack 存取 |
| `common/` | Profile、CRC、隨機数、文字及 SD 共用工具 |

新檔案放在擁有責任的目錄，`include/` 與 `src/` 對應；共用 decoder 內部介面放 `detail/`。
第三方庫由 PlatformIO 管理，不為目錄對稱增加 adapter／port。

## 主要路徑

- 啟動：`GameStartup → PetSession → RuntimeContractLoader`，載入 manifest、初始外觀及契約；
  匹配存檔時恢復，否則建立新局。Game 啟用顯示、命令與播放。
- 命令：`CommandController → CommandExecutor → PetBehaviorRuntime` 或 System Command。
  Action 先原子提交數值與暫停，再提交完整播放序列。
- 換日：RTC／SessionClock／PetClock 提供日邊界，Game 執行一次日交易；
  預設啟用的每日後處理協調解鎖、保存與 Evolution。
- 外觀：`AppearanceChangeController` 負責套用與保存；`EvolutionController`
  負責進化播放與轉換時機，手動選單由 `AppearanceSelectionController` 處理。
- 畫面：`LayoutRenderer` 同步版面，`AnimationController` 選影格，
  `BundleReader` 定位 payload，`FrameDecoder` 串流送 TFT。

現行韌體接受 `/runtime.bin` v19；格式由 Web 規格管理。
資料／素材完整驗證在 exporter。韌體保留必要讀取、容量與硬體失敗處理，
記錄第一個資源錯誤。Fatal 關閉互動、取消暫態工作，重開機才恢復。
不要把歷史重構計畫或當時的 Flash 數字當成尚待執行工作。
