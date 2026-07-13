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
| 本輪前 `kuromu` | 58396 bytes | 89.1% | 10264 bytes | 50.1% |
| 本輪 RLE/固定緩衝後 `kuromu` | 55716 bytes | 85.0% | 9152 bytes | 44.7% |
| Sparse registry 後 `kuromu` | 55980 bytes | 85.4% | 8728 bytes | 42.6% |

首輪共節省 Flash：11072 bytes，約為 64 KB flash 空間的 16.9%。RLE/固定緩衝階段在 `kuromu` 另節省 Flash 2680 bytes、RAM 1112 bytes。Sparse registry 再降低 `kuromu` RAM 424 bytes，代價是 Flash 增加 264 bytes。

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

5. 本輪固定韌體 runtime 為 RLE renderer。
   - 移除 `BMPRenderer` 與韌體端 BMP decoder 分支。
   - 資源工具仍可保留 BMP 作為中間格式，但 `index.txt` runtime 格式必須是 `rle`。

6. 本輪壓縮常駐資料與移除 heap 容器。
   - `AnimationMeta` path buffer 從 64 bytes 降到 48 bytes，並移除 runtime 不再需要的 asset format 欄位。
   - Renderer RLE read/line buffer 改成固定容量陣列，維持 12-line TFT batch draw。
   - Animation queue 從 `std::deque` 改成固定 8 筆 priority queue。
   - Manifest path 過長時會在 TFT 顯示 `path error`.

7. 將 `gAnimationRegistry` 改為 sparse registry。
   - 保留既有 `AnimationId` enum 與 `metaFor(AnimationId)` 呼叫方式。
   - Registry 只存實際載入的 enum animation，容量為 48 筆。
   - `None` 或未載入動畫共用一筆空 metadata，不再為所有 enum ID 常駐保留 slot。
   - Registry 容量不足時會在 TFT 顯示 `registry full`。

## 驗證

目前全環境 size build 已成功完成：

```powershell
platformio run -e default -e new_taipei_childrens_day -e kuromu -e small_multi_status -e small_status_anime -e dipsyho -e small -e small_start -e stm32 -t size
```

目前 `kuromu` PlatformIO 記憶體報告：

```text
text=55740, data=240, bss=8488
Flash: 55980 bytes
RAM:   8728 bytes
```

目前各環境 ELF size：

| Env | Flash text+data | RAM data+bss |
| --- | ---: | ---: |
| `default` | 57956 bytes | 8728 bytes |
| `new_taipei_childrens_day` | 55540 bytes | 8728 bytes |
| `kuromu` | 55980 bytes | 8728 bytes |
| `small_multi_status` | 57920 bytes | 8728 bytes |
| `small_status_anime` | 56172 bytes | 8728 bytes |
| `dipsyho` | 57856 bytes | 8728 bytes |
| `small` | 57892 bytes | 8728 bytes |
| `small_start` | 58040 bytes | 8728 bytes |
| `stm32` | 57956 bytes | 8728 bytes |

## 注意事項與風險

- FPS report 檔案產生功能目前在 release build 中停用。若要用於 profiling，請設定 `-DENABLE_RENDER_STATS=1`。
- 序列埠開機訊息已移除。
- 韌體 runtime 只支援 RLE SD assets；manifest 中非 `rle` 的行會被忽略。
- Manifest path buffer 目前為 48 bytes。若路徑超過上限，載入 manifest 時會顯示 `path error`。
- Enum animation sparse registry 目前容量為 48 筆。若 manifest 實際載入動畫超過容量，會顯示 `registry full`。
- Pet state 目前使用雙 slot、sequence number 與 bitwise CRC32，會增加少量程式碼與 state record 大小，但可提升斷電後恢復機率。
- LTO 可能稍微增加建置時間，但本次產生了顯著的 Flash 降幅，而且韌體仍可成功建置。
- 剩餘 warning 來自 Adafruit ST77xx library 使用 Arduino 已棄用的 `boolean` typedef；這不是本次最佳化造成的。

## 後續最佳化建議

- Renderer 緩衝與 animation queue 已改為固定容量，避免相關 heap fragmentation。
- 若資源與狀態檔格式可以保持簡單，可檢視 `SdFat` 設定，關閉未使用的 exFAT、iostream 或 long filename 支援。
- 韌體 runtime 固定只支援 SD RLE 資源；BMP 可保留作為資源工具中間格式。
- 生產版韌體建議維持停用 render stats。
