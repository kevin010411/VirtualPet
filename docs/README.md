# 韌體文件

本目錄只記錄現行韌體實作、操作與未完成工作。跨層產品規格由 Web 維護：

- [Pet Stats／Behavior／Action／Status／Evolution／存檔](E:/C++/virtualPet/web/.scratch/sd-driven-pet-stats/spec.md)
- [Runtime Table](E:/C++/virtualPet/web/.scratch/binary-runtime-tables/spec.md)
- [播放序列音訊](E:/C++/virtualPet/web/.scratch/sequence-audio/spec.md)

| 需要做什麼 | 文件 |
| --- | --- |
| 匯出、建置與驗證 | [開發流程](development-workflow.md) |
| 查功能開關與產生來源 | [Profile flags](profile_feature_flags.md) |
| 找模組與執行路徑 | [架構](architecture.md) |
| 查播放／資源／保存 | [動畫](animation_scheduler.md)、[資源](renderer_asset_formats.md)、[存檔](pet_storage.md) |
| 查死亡與音訊執行 | [死亡](death_runtime.md)、[音訊](sequence-audio-verification.md) |
| 查硬體與待機限制 | [硬體](hardware/power-strategy-2026-10-09.md)、[RTC／按鍵／STOP](hardware/rtc-input-recovery.md) |
| 查尚未完成的需求 | [功能待辦](bingbing-development-todo.md)、[兵兵差異](bingbing-v1.9.4-feature-gaps.md)、[四款遊戲提案](minigame-plans/README.md) |
| 查尺寸量測與未實作旗標 | [Flash 評估](flash-feature-audit.md) |

可選功能旗標在 header 中預設 `0`，由 Web Profile Resolver 產生的 ini 明確開啟。
`ENABLE_DAILY_POST_PROCESSING` 是診斷例外，預設 `1` 保留正常每日生命週期。
SD 資料與圖片驗證由 Web backend/exporter 負責；韌體只處理有界讀取、
固定容量與實際 SD／硬體執行失敗。Host／建置結果與實機驗收分開記錄。

已合併或移除的舊文件見[清理紀錄](cleanup-review.md)。
