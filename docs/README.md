# 文件索引

## 從這裡開始

- [開發與建置指令](codex.md)
- [應用程式 profile 與功能開關](app_profiles.md)
- [Profile feature flags 中文查表](profile_feature_flags.md)
- [Profile size 驗證](profile_size_verification.md)
- [系統架構](architecture.md)

## SD 卡設定與資源

Pet Stat、Status 與 Evolution runtime table 請由 Web exporter 產生；權威規格位於
`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`。本 repository 不再
提供 `state_schema.txt`、Pet Stat alias 或手寫 Status contract 範例。

[sd_card_example](sd_card_example/README.md) 只保留韌體資源布局提示。

- `runtime.bin`：完整的 versioned runtime table，不接受 TXT fallback。
- `index/`：舊版動畫 manifest 範例，僅供歷史對照。

相關格式說明：

- [舊版 `/index/` manifest 格式（歷史資料）](index_txt_format.md)
- [Renderer 資源格式](renderer_asset_formats.md)
- [寵物存檔格式](pet_storage.md)

## 維護與歷史資料

- [Flash 最佳化紀錄](flash_optimization_report.md)
- [待辦事項](Todo.md)
- `legacy/`：舊版硬體與 PlatformIO 設定，僅供參考。

舊版 `/index/` 與 `.txt` 範例只供歷史對照；目前專案的 runtime 設定應由 Web exporter 產生，不能依舊範例手寫。
