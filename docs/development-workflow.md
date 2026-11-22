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
pwsh -NoProfile -NonInteractive -File test/run_host_tests.ps1 -List
pwsh -NoProfile -NonInteractive -File test/run_host_tests.ps1 -Case pet-behavior
```

完成建置後，以 `arm-none-eabi-size`／`nm` 檢查該 env 的新 ELF。
Flash = `text + data`，靜態 RAM = `data + bss`。A/B 必須固定來源、工具鏈、
board、容量及其餘 flags，不能把歷史 project 數字直接相減。
靜態 RAM 不包含 heap／stack 峰值；擴張 linker allowance 不證明實體 Flash 容量。

實機需另驗收 SD／TFT／按鍵、RTC／STOP、低電壓及音訊輸出。
軟體通過不表示上述設備行為已通過。未完成需求集中在[功能待辦](bingbing-development-todo.md)。
