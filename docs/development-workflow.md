# 開發、匯出與驗證

1. 先確認本批需求、責任與驗收，再改程式；跨層契約更新在 Web repository。
2. 在 Web 保存並套用專案，匯出同一快照的 SD bundle 與 Profile Resolver ini。
3. 使用產生的 ini 建置；不要依舊客戶名稱、文件範例或手改 header 啟用功能。
4. 執行受影響 host 檢查，使用同一份 Web 匯出 fixture 驗證跨層行為。
5. 記錄實際建置與實機驗收範圍，保留工作樹中其他任務的變更。

## 建置

完整 resolver 產物可替換 `platformio.ini`，或另存為完整設定並以 `-c` 指定。
舊式只含一個 env 的 `profile.ini` 片段需併入其指定的基底；不能當成完整 ini。

```powershell
# 使用目前 platformio.ini 的 default_envs，避免寫死 Project ID。
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run
# 指定另一份完整 resolver 設定。
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -c path\to\platformio.ini
```

將匹配版本的 `runtime.bin`、`runtime.manifest` 與 `.data` packs 一起放入 SD；
音訊專案另含 `audio.data`。韌體不接受舊 TXT runtime fallback，更新二進位版本後需重新匯出。

## 驗證與尺寸

Host 入口在 `test/<功能>/run_host_test.ps1`；選項以各 script 的 `param` 為準。
受旗標控制的行為需測開／關組態。此 repository 沒有 `scripts/verify.ps1`；
Web 修改則依 Web 的 `AGENTS.md` 執行 canonical preflight／verify。

```powershell
pwsh -NoProfile -File test/pet_behavior_runtime/run_host_test.ps1
pwsh -NoProfile -File test/runtime_table_behavior/run_host_test.ps1
pwsh -NoProfile -File test/game_startup/run_host_test.ps1 -EvolutionOnly
```

完成建置後，以 `arm-none-eabi-size`／`nm` 檢查該 env 的新 ELF。
Flash = `text + data`，靜態 RAM = `data + bss`。A/B 必須固定來源、工具鏈、
board、容量及其餘 flags，不能把歷史 project 數字直接相減。
靜態 RAM 不包含 heap／stack 峰值；擴張 linker allowance 不證明實體 Flash 容量。

實機需另驗收 SD／TFT／按鍵、RTC／STOP、低電壓及音訊輸出。
軟體通過不表示上述設備行為已通過。未完成需求集中在[功能待辦](bingbing-development-todo.md)。
