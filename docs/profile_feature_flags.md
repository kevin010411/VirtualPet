# `project_12` Profile flags

本頁列出編譯期功能開關；目前 `platformio.ini` 的環境為 `project_29`，實際覆寫以該檔案為準，其他可用預設值見 `include/common/AppProfile.h`。專案環境由 Profile Resolver 產生，修改後須用實際產生的設定重新建置。`APP_STATUS_MODE`、`STATUS_MODE_AGE` 等舊固定照護模式不是現行 Status Sets 的設定方式。

| Flag | `project_12` 值 | 用途 |
| --- | --- | --- |
| `ENABLE_DEBUG` | `0` | 關閉除錯輸出與量測檔寫入 |
| `ENABLE_GUESS_GAME` | `1` | 編入猜物小遊戲 |
| `ENABLE_GUESS_GAME_SINGLE_ROUND` | `1` | 單回合流程 |
| `ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT` | `1` | 使用左右選擇結果 |
| `ENABLE_STARTUP_ANIMATION` | `1` | 編入啟動動畫流程 |
| `ENABLE_FIRST_START_ANIMATION` | `0` | 不播放首次啟動專用動畫 |
| `ENABLE_COMMAND_OUTFIT` | `0` | 不編入直接換裝 System Command |
| `ENABLE_DYNAMIC_ACTION_LAYOUT` | `0` | 不編入動態 Action Layout 路徑 |
| `ENABLE_COMMAND_PREDICT` | `0` | 不編入 Predict System Command |
| `ENABLE_SEQUENTIAL_STATUS_SET_SELECTION` | `0` | Status Set 使用隨機選擇 |
| `APP_MAX_LOADED_ANIMATIONS` | `11` | 載入動畫容量上限 |
| `APP_MAX_NAMED_ANIMATIONS` | `1` | 命名動畫容量上限 |
| `APP_MAX_ANIMATION_VARIANTS` | `11` | 動畫版本容量上限 |

尺寸以 linked ELF 計算：Flash 為 `text + data`，靜態 RAM 為 `data + bss`。調整 flags 或容量時，記錄同一環境前後數字；Flash 減少不能替代功能與資源驗證。第三方圖形與 SdFat 依賴仍需保留。
