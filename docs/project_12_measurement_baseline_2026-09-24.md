# project_12 量測基準（2026-09-24）

本文件記錄目前韌體的可重現建置基準，以及尚待實機取得的效能數據。量測對象是 Git HEAD `d141db0`、乾淨工作樹、`platformio.ini` 的 `project_12` 環境（STM32F103C8T6，64 KiB Flash、20 KiB RAM）。未改動韌體原始碼或 profile。

## 已量測：完整重建與連結尺寸

在 PowerShell 執行：

```powershell
pio run -e project_12 -t clean
pio run -e project_12
& 'C:\Users\kevin\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-size.exe' -A '.pio\build\project_12\firmware.elf'
Get-FileHash -Algorithm SHA256 '.pio\build\project_12\firmware.elf'
Get-FileHash -Algorithm SHA256 '.pio\build\project_12\firmware.bin'
```

完整重建成功。PlatformIO 使用 ST STM32 19.0.0、Arduino STM32 2.9.0、GCC ARM 12.3.1，以及 Adafruit GFX 1.12.6、Adafruit ST7735/ST7789 1.11.0、SdFat Adafruit Fork 2.3.103。結果：

| 指標 | 值 | 口徑 |
| --- | ---: | --- |
| PlatformIO Flash | 61,496 / 65,536 B（93.8%） | `.text + .rodata + .data`，PlatformIO 的顯示值 |
| `.bin` 大小 | 61,792 B | 實際輸出的連續 Flash image；與 PlatformIO Flash 口徑不同 |
| PlatformIO RAM | 6,840 / 20,480 B（33.4%） | `.data + .bss`，不代表執行時峰值 |
| `.text` | 56,792 B | ELF section |
| `.rodata` | 4,464 B | ELF section |
| `.data` | 240 B | ELF section |
| `.bss` | 6,600 B | ELF section |
| linker `_user_heap_stack` | 1,536 B | 連結保留量；不是實測 heap 或 stack 峰值 |

產物：`.pio/build/project_12/firmware.elf`、`firmware.map`、`firmware.bin`。ELF SHA-256：`DBC4300C1D7F28AA37917F46592220D79E24D33943F9F879ED45838FB49306A1`；BIN SHA-256：`0D9C1B58D019E0879CC45AF5875554A7ABF5F3A48074809208BE0A6877D55590`。

## 待量測：實機效能

本次環境未找到交接文件提到的 `project-12-sd-card` 目錄，也未偵測到序列埠或可由電腦掛載的 SD 卡。因此尚無固定的 SD bundle、實機啟動／繪圖時間、heap 峰值或 stack watermark。不可從 ELF 或主機測試推算這些數值。

後續已透過 ST-Link V2（VID:PID `0483:3748`）成功連上 Cortex-M3 目標，OpenOCD 回報目標電壓約 3.19 V。使用 `verify_image .pio/build/project_12/firmware.bin 0x08000000` 唯讀比對，命令以 0 結束且未報不符；比對時短暫 halt 後已 resume。ST-Link 連線本身不提供現有韌體的啟動歷史、逐幀時間或 SD 上的報表。

下次使用同一塊 STM32F103C8 板、同一張 SD 卡與同一份匯出 bundle，記錄 bundle 的來源、檔案雜湊、SD 卡型號、顯示器和供電條件。每種情境至少冷啟動 10 次，取中位數與最大值：正常啟動到第一幀、連續同動畫至少 100 幀、切換動畫與 pack、Status／User Action、首次使用未快取資源。保留每次原始紀錄與失敗次數。

現有 `ENABLE_DEBUG` 分支可寫入 SD 根目錄 `/boot_timing.txt`，包含 `sd_init_ms`、`game_prepare_ms`、`game_render_ms`、`startup_ready_ms`、`first_frame_ms`；`/render_fps.txt` 記錄累計／視窗繪圖耗時換算的 FPS 及最後一幀耗時。`project_12` 目前設定 `ENABLE_DEBUG=0`，啟用除錯會產生另一個韌體，尺寸與效能須與上述 release 基準分開記錄。這些現有報表沒有拆出每幀 pack open/index/read、解碼與 TFT 傳輸，也沒有 heap／stack 峰值；要比較相關優化，須先以低干擾量測補齊這些欄位，並保留未加量測碼的 release build 作尺寸對照。

實機紀錄表（尚未填值）：

| 情境／指標 | 中位數 | 最大值 | 樣本數 | bundle SHA-256 | 韌體 SHA-256 |
| --- | ---: | ---: | ---: | --- | --- |
| 冷啟動到第一幀（ms） | 待量測 | 待量測 | 0 | 待指定 | 待指定 |
| 每幀 pack open/index/read（µs） | 待量測 | 待量測 | 0 | 待指定 | 待指定 |
| 每幀解碼（µs） | 待量測 | 待量測 | 0 | 待指定 | 待指定 |
| 每幀 TFT 傳輸（µs） | 待量測 | 待量測 | 0 | 待指定 | 待指定 |
| heap 峰值／stack 最低餘量（B） | 待量測 | 待量測 | 0 | 待指定 | 待指定 |

比較重構前後數據時，固定板、SD 卡、bundle、場景與量測韌體設定；Flash／靜態 RAM 使用完整重建值，執行時記憶體與延遲使用實機測值。
