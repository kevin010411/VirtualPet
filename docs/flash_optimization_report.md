# Flash 最佳化報告

## 目標

- 開發板：`genericSTM32F103C8`
- MCU：STM32F103C8T6
- Flash 限制：64 KB
- RAM 限制：20 KB
- 建置指令：`platformio run`

## 結果

| 建置狀態 | Flash | Flash 使用率 | RAM | RAM 使用率 |
| --- | ---: | ---: | ---: | ---: |
| 最佳化前 | 60076 bytes | 91.7% | 7848 bytes | 38.3% |
| 最佳化後 | 49504 bytes | 75.5% | 9352 bytes | 45.7% |

共節省 Flash：11072 bytes，約為 64 KB flash 空間的 16.9%。

## 已套用的變更

1. 在 `platformio.ini` 啟用偏向 release 與程式大小的建置 flags。
   - `-Os` 要求 GCC 以程式碼大小為目標進行最佳化。
   - `-flto` 讓 linker 可以跨 translation unit 與 library 移除更多未使用程式碼。
   - `-ffunction-sections` 與 `-fdata-sections` 讓函式與資料可被 linker garbage collection 分開處理。
   - `-fno-exceptions` 避免在這個嵌入式目標上編入 C++ exception 支援碼。

2. 預設停用 render FPS 報表。
   - 新增 `ENABLE_RENDER_STATS=0`。
   - 保留 reporter 程式碼，並放在 flag 後方。
   - 功能停用時，從 `Renderer` 移除 release 期間的 `micros()`、float 與 FPS 更新路徑。
   - 主要 Flash 節省來源：避免 release 韌體編入 float 運算、格式化 float 輸出，以及 SD 報表寫入邏輯。

3. 移除序列埠開機 log。
   - 刪除 `Serial.begin()` 與 `Serial.println()` 呼叫。
   - release 韌體不再編入開機序列埠輸出路徑。

4. 移除未使用的 `Adafruit EPD` 依賴。
   - 專案使用的是 `Adafruit_ST7735`，不是 EPD/ThinkInk 類別。
   - 移除後可降低 dependency scan / build 範圍，也避免未來意外拉入相關程式碼。

## 驗證

變更後 `platformio run` 已成功完成。

最終 PlatformIO 記憶體報告：

```text
text=49256, data=248, bss=9104
Flash: 49504 bytes
RAM:   9352 bytes
```

## 注意事項與風險

- FPS report 檔案產生功能目前在 release build 中停用。若要用於 profiling，請設定 `-DENABLE_RENDER_STATS=1`。
- 序列埠開機訊息已移除。
- LTO 可能稍微增加建置時間，但本次產生了顯著的 Flash 降幅，而且韌體仍可成功建置。
- 剩餘 warning 來自 Adafruit ST77xx library 使用 Arduino 已棄用的 `boolean` typedef；這不是本次最佳化造成的。

## 後續最佳化建議

- 若 Flash 或 heap fragmentation 成為問題，可將 `std::vector` 與 `std::deque` 改為固定容量 buffer。
- 若資源與狀態檔格式可以保持簡單，可檢視 `SdFat` 設定，關閉未使用的 exFAT、iostream 或 long filename 支援。
- SD 資源格式可透過 `ENABLE_SD_BMP_ASSETS` / `ENABLE_SD_RLE_ASSETS` 在 build time 選擇，預設只編入 RLE renderer。
- 生產版韌體建議維持停用 render stats。
