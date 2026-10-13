# Profile 功能旗標

`include/common/AppProfile.h` 是所有 `ENABLE_*` fallback 的唯一來源。
14 個可選功能旗標預設 `0`；診斷開關 `ENABLE_DAILY_POST_PROCESSING` 預設 `1`。
正式功能由 Web `backend/services/feature_sets/profile_resolver.py` 產生完整 ini 開啟，
每日後處理使用 header 的正常預設，resolver 不輸出此診斷開關。
實際覆寫以本次產生的 `platformio.ini` 為準。

| Flag | Resolver 的開啟依據／用途 |
| --- | --- |
| `ENABLE_DAILY_POST_PROCESSING` | 預設 `1` 保留每日解鎖、圖鑑結算、定期保存與自動進化；resolver 不輸出，診斷／尺寸比較才覆寫 `0` |
| `ENABLE_GUESS_GAME` | Project 的 `guess_game` 功能 |
| `ENABLE_GUESS_GAME_SINGLE_ROUND` | Guess Game 的單回合設定 |
| `ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT` | Guess Game 的 `player_choice` 結果模式 |
| `ENABLE_STARTUP_ANIMATION` | Project 的 `startup` 功能 |
| `ENABLE_FIRST_START_ANIMATION` | 有效的 FirstStart 設定；須同時開啟 startup |
| `ENABLE_COMMAND_OUTFIT` | Project 的 `change_outfit` 功能 |
| `ENABLE_APPEARANCE_SELECTION` | 同一 `change_outfit` 功能；不再隨 command 巨集自動開啟 |
| `ENABLE_COLLECTION_CATALOG` | Project 的 `collection_catalog` 功能，包含 Action 次數追蹤 |
| `ENABLE_DEATH` | Project 的 `death.enabled` |
| `ENABLE_SEQUENCE_AUDIO` | 實際使用的播放序列音訊；只開啟付費功能但沒有使用音訊時仍為 `0` |
| `ENABLE_SEQUENTIAL_STATUS_SET_SELECTION` | Status Set 的 `sequential` 模式；`0` 使用隨機選擇 |
| `ENABLE_COMMAND_PREDICT` | 現行 resolver 固定 `0` |
| `ENABLE_OUTFIT_CHOOSE_ANIMATION` | 現行 resolver 未輸出，保持 `0` |
| `ENABLE_DEBUG` | 正式 resolver 基底固定 `0`；除錯輸出與量測寫檔 |

每日後處理關閉時，日數與 Pet Stat Daily Change 仍推進，但不做上述每日後處理。
手動外觀選擇關閉不移除 Evolution／圖鑑共用的外觀切換路徑。
圖鑑關閉保留固定存檔欄位；死亡關閉仍接受已死亡存檔的按鍵新局，見[死亡執行](death_runtime.md)。

`APP_MAX_PET_STATS` 與 `APP_MAX_SCREEN_RULES` 是容量，不是功能旗標，不能設成 `0`。
Resolver 目前輸出 Stats=12、Screen Rules=已套用版面需要的數量（至少 1）。
其他固定容量由對應模組管理。開換裝時必須同時開外觀選擇；header 保留編譯期相依檢查。

現有 `ENABLE_ACTION_PLAYBACK_SEQUENCE`／`ENABLE_NUMERIC_SCREEN_LAYOUT` 只是[評估提案](flash-feature-audit.md)，
尚未實作，不能在 ini 中使用。新增 flag 時同步更新 resolver 與開／關驗證。
