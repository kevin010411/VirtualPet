# 文件索引

## 從這裡開始

- [開發與建置指令](codex.md)
- [應用程式 profile 與功能開關](app_profiles.md)
- [Profile feature flags 中文查表](profile_feature_flags.md)
- [Profile size 驗證](profile_size_verification.md)
- [系統架構](architecture.md)

## SD 卡設定與資源

請使用 [sd_card_example](sd_card_example/README.md) 作為**唯一建議複製來源**。該資料夾的註解均為中文，且檔案結構對應 SD 卡根目錄。

- `custom_rules.txt`：全域 custom stat、每日變化與指令額外效果。
- `evolution_rules.txt`：演化規則。
- `state_schema.txt`：custom stat 別名與範圍。
- `status_display.txt`：狀態畫面設定。
- `index/`：動畫 manifest 範例。

相關格式說明：

- [動畫 manifest 格式](index_txt_format.md)
- [Renderer 資源格式](renderer_asset_formats.md)
- [寵物存檔格式](pet_storage.md)

## 維護與歷史資料

- [Flash 最佳化紀錄](flash_optimization_report.md)
- [待辦事項](Todo.md)
- `legacy/`：舊版硬體與 PlatformIO 設定，僅供參考。

根目錄中舊有的 `.txt` 範例仍暫時保留以維持舊連結；新增或更新 SD 設定時，請只修改 `sd_card_example/` 內的版本。
