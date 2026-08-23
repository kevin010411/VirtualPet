# 文件索引

## 從這裡開始

- [開發與建置指令](codex.md)
- [應用程式 profile 與功能開關](app_profiles.md)
- [Profile feature flags 中文查表](profile_feature_flags.md)
- [Profile size 驗證](profile_size_verification.md)
- [系統架構](architecture.md)

## SD 卡設定與資源

Pet Stat、Status 與 Evolution runtime contract 請由 Web exporter 產生；權威規格位於
`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`。本 repository 不再
提供 `state_schema.txt`、Pet Stat alias 或手寫 Status contract 範例。

[sd_card_example](sd_card_example/README.md) 只保留韌體資源布局提示。

- `evolution_rules.txt`：演化規則。
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
