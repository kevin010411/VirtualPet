# 開發、匯出與驗證

## 建置 repo 基準

從韌體 repo 根目錄操作。安裝 PlatformIO Core 6.2.0，確認 `pio` 在 PATH 上。
首次建置需要下載平台、工具鏈及函式庫。
[`platformio.ini`](../platformio.ini) 固定基準的平台、編譯工具鏈和函式庫版本，
並沿用已簽入的 `project_59` 功能及容量組合。

```powershell
pio --version
pio run
```

成功後可在 `.pio/build/project_59/` 找到 `firmware.elf` 與 `firmware.bin`。
此組態供 checkout 建置和比較使用。交付裝置時使用下方的 Web 專案產物。

## 匯出與建置 Web 專案

1. 在 Web 儲存專案變更並套用版面。
2. 從管理後台為同一專案快照下載「專案設定包」與「SD 卡素材包」。匯出設定包時，依實際裝置選擇 MCU 與靜音設定。
3. 將設定包中的 `platformio.ini` 解壓到獨立目錄。確認 `default_envs` 指向本次要建置的 `project_<id>`。
4. 把 SD 卡素材包解壓至 SD 卡根目錄，保留 `assets/` 路徑和各功能資料檔。
   更新執行契約時，重新匯出完整套件。[SD 檔案配置](renderer_asset_formats.md)
5. 在韌體根目錄使用 `-c` 建置完整的 Resolver ini，將路徑換成步驟 3 的檔案。

```powershell
pio run -c 'path/to/exported/platformio.ini'
```

Resolver 根據專案功能、MCU 與容量產生完整設定，並有自己的相依宣告。
要重跑同一交付版本，保留兩份 ZIP、韌體 Git revision 和實際解析的套件版本。
舊式單一 env 的 `profile.ini` 片段需搭配該片段原本的基底。
[契約與 Resolver 來源](README.md#跨端規格) · [韌體功能旗標](profile_feature_flags.md)

## 驗證與尺寸

依功能選擇 host 案例和相符的 Web 匯出 fixture。
受旗標控制的行為應驗證適用的開啟和關閉組態。[Host 測試清單](../test/README.md)

```powershell
pwsh -NoProfile -NonInteractive -File test/run_host_tests.ps1 -Core
python tools/profile_size_report.py --check-capacity
```

容量驗收使用 PlatformIO 建置下列組態，從本次 ELF 的 `text + data` 計算 Flash。
建置失敗或超出任一門檻都會回傳非零退出碼。

| 組態 | 功能 | Flash 驗收 |
| --- | --- | --- |
| `stm32_base` | STM32F103C8T6。關閉可選功能與按鍵音，保留日結算、存檔與基本播放。 | 嚴格低於 64 KiB 的 90%，最多 58,982 bytes。 |
| `gd32_full` | GD32F103CBT6 108 MHz。啟用目前所有可選遊戲與產品功能，含音訊及兩代口令玩法。關閉除錯輸出。 | 不超過 128 KiB。 |

`gd32_full` 使用 12 個 Pet Stat、16 條版面數值規則、3 個 Action、8 個 Outcome 與 12 筆常駐 Action Effect。
新增可選功能時，在 `gd32_full` 開啟對應旗標。`stm32_base` 使用 `AppProfile.h` 的關閉預設。
交付時用該專案匯出的 `platformio.ini` 建置，確認實際資料容量所需的空間。
同 checkout 若需平行建置，為每個程序指定不同的 `PLATFORMIO_BUILD_DIR`。

用 `arm-none-eabi-size -A` 檢查 ELF 的 `.data` 和 `.bss` 可核對靜態 RAM。
工具鏈的 Berkeley `bss` 總計可能包含 linker 預留的 heap 與 stack。實際 heap 與 stack 峰值須另量測。
同組態比較須固定板型、工具鏈、儲存容量與其餘旗標。

實機需另驗收 SD／TFT／按鍵、RTC／STOP、低電壓及音訊輸出。
驗收 STM32 在喚醒和低電壓切換後的背光亮度，以及 GD32 連續動畫配樂的 DMA 補樣。
軟體通過不表示上述設備行為已通過。未完成需求集中在[功能待辦](bingbing-development-todo.md)。
