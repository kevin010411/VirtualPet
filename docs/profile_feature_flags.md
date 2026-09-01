# Profile feature flags 中文查表

這份文件列出 `platformio.ini` 與 `include/shared/config/AppProfile.h` 目前可用的 profile flag。調整 profile 時，請先確認這裡的用途與驗收方式，再跑 `docs/profile_size_verification.md` 的 size 檢查。

重要觀念：

- Flash 驗收看 `text + data`。
- RAM 驗收看 `data + bss`。
- 關閉 function body 通常只省 Flash；要讓 RAM 下降，必須排除 static/global buffer、class member，或降低固定陣列容量。
- 本專案採保守式策略：保留 SD 卡圖片與 manifest 動態讀取，不把 manifest 固定寫死到程式。

## Profile 與狀態模式

| Flag | 預設值 | 中文說明 | 開啟/設定後會編譯 | 關閉或改小可省什麼 | 目前使用 profile | 注意事項 |
| --- | --- | --- | --- | --- | --- | --- |
| `APP_PROFILE` | `APP_PROFILE_DEFAULT` | 指定 profile id，影響 command slot 排列與初始外觀。 | 對應 profile 的 command slot 與初始 species/outfit。 | 不直接省容量；真正省容量要靠其他 `ENABLE_*` 與 `APP_MAX_*`。 | 所有 env 都會設定。 | 新增 profile 時要同步更新 `AppProfile.h` 與 `platformio.ini`。 |
| `APP_STATUS_MODE` | `STATUS_MODE_SINGLE_METER` | 指定 Status 指令的顯示策略。 | `STATUS_MODE_DIRECT`、`SINGLE_METER`、`RANDOM_METERS`、`COMPOSITE_HEALTH`、`TRIPLE_METER` 其中一種。 | 選 `DIRECT` 或 `COMPOSITE_HEALTH` 可避開 SD status config parser。 | `default`、`kuromu`、`small`、`new_taipei_childrens_day`、`dipsyho`。 | `small` 不是小容量樣本，因為它仍開 guess game、appearance、species。 |
| `APP_FIRST_LAUNCH_REQUIRED_COMMAND` | `APP_COMMAND_CHANGE_OUTFIT` | 第一次啟動流程要求完成的 Outfit command。 | first-launch flow 只會在 Initial Species 上等待 Outfit Selection。 | 不直接省容量。 | 目前使用預設值。 | 必須啟用 `ENABLE_APPEARANCE_SELECTION`；指定已退役的 Species command 會編譯失敗。 |

## Command 與功能模組

| Flag | 預設值 | 中文說明 | 開啟時會編譯 | 關閉時預期可省 Flash/RAM | 目前使用 profile | 注意事項 |
| --- | --- | --- | --- | --- | --- | --- |
| `ENABLE_COMMAND_PREDICT` | `1` | 是否啟用占卜指令。 | Predict slot、`canPredict()`、`commandPredict()`、fortune helper、Predict animation 檢查。 | 主要省 Flash。 | `default=1`，其他驗收 profile 多為 `0`。 | 若關閉後 Flash 沒降，用 symbol 檢查 `commandPredict` / `fortuneToAnimationId`。 |
| `ENABLE_COMMAND_OUTFIT` | `1` | 是否啟用換 outfit 指令。 | ChangeOutfit slot、result handler、appearance selection 入口。 | 主要省 Flash。 | `kuromu=1`。 | 開啟時必須啟用 `ENABLE_APPEARANCE_SELECTION`。 |
| `ENABLE_COMMAND_SPECIES` | `0` | 僅供舊版相容的直接換 Species 指令；此路徑會繞過 Evolution。 | ChangeSpecies slot、result handler、species selection 入口。 | 主要省 Flash。 | 預設與正式產品設定皆為 `0`；手動開啟會產生編譯警告。 | 開啟時必須啟用 `ENABLE_APPEARANCE_SELECTION`，且不屬於支援的產品功能。 |
| `ENABLE_GUESS_GAME` | `1` | 是否啟用猜物品小遊戲。 | `GuessItemGame`、`MinigameController`、minigame input/update flow、HaveFun slot。 | 主要省 Flash；也會讓 `Game` 少一個 minigame member。 | `default`、`small`、`dipsyho`。 | `new_taipei_childrens_day` 與 `kuromu` 目前關閉。 |
## Appearance、啟動與 layout

| Flag | 預設值 | 中文說明 | 開啟時會編譯 | 關閉時預期可省 Flash/RAM | 目前使用 profile | 注意事項 |
| --- | --- | --- | --- | --- | --- | --- |
| `ENABLE_APPEARANCE_SELECTION` | `1` | 是否啟用外觀選擇 controller。 | `AppearanceSelectionController` 與 Game 中外觀選擇流程。 | 主要省 Flash；`Game` 也會少一個 unique_ptr member。 | `kuromu`、`small`、`new_taipei_childrens_day`。 | outfit/species command 或 first-launch selection 需要它。 |
| `ENABLE_STARTUP_ANIMATION` | `0` | 是否啟用開場動畫。 | Start animation flow。 | 主要省 Flash。 | `new_taipei_childrens_day`、`small_start`。 | 只關閉此 flag 不會移除 SD manifest 讀取。 |
| `ENABLE_FIRST_LAUNCH_SELECTION` | `0` | 是否啟用第一次啟動必選外觀流程。 | first-launch stage 與 required command flow。 | 主要省 Flash。 | `new_taipei_childrens_day`。 | 開啟時必須啟用 `ENABLE_APPEARANCE_SELECTION`。 |
| `ENABLE_OUTFIT_CHOOSE_ANIMATION` | `0` | 是否讓 outfit 選擇流程使用 choose animation。 | 外觀選擇中的 outfit preview/choose 行為。 | 主要省 Flash。 | `new_taipei_childrens_day`、`kuromu`。 | 需搭配外觀資源。 |
| `ENABLE_DYNAMIC_ACTION_LAYOUT` | `0` | 是否啟用動態 action layout。 | 動態 layout path/slot 邏輯。 | 主要省 Flash。 | `kuromu`。 | 沒有動態 layout 資源的 profile 不要開。 |

## Manifest RAM 容量

| Flag | 預設值 | 中文說明 | 設定後會配置 | 改小可省什麼 | 目前使用 profile | 注意事項 |
| --- | --- | --- | --- | --- | --- | --- |
| `APP_MAX_LOADED_ANIMATIONS` | `48` | `AssetManifest` 可載入的 enum animation 最大數。 | `.bss` 中的 `gAnimationRegistry` 固定陣列。 | 直接省 RAM `.bss`。 | `default=48`、`small=44`、`kuromu=32`、`new_taipei_childrens_day=24`、`dipsyho=28`。 | 設太小會造成 manifest capacity error。調小後務必實機確認資源可載入。 |
| `APP_MAX_NAMED_ANIMATIONS` | `4` | 非 enum 命名動畫最大數。 | `.bss` 中的 named animation name/meta buffer。 | 直接省 RAM `.bss`。 | 多數 profile 設為 `2`，`kuromu=4`。 | custom rules 可能會用非 enum 動畫，例如庫洛姆的特殊 action。 |
| `APP_MAX_ANIMATION_VARIANTS` | `16` | variant animation 最大數，例如 `Gift1`、`Gift2`。 | `.bss` 中的 variant animation buffer。 | 直接省 RAM `.bss`。 | 多數 profile 設為 `2`，`kuromu=8`。 | 若 gift/custom action 有多個 variant，不能設太小。 |

## 驗收方式

調整任何 flag 後，至少跑：

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e default -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e kuromu -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e small -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e new_taipei_childrens_day -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e dipsyho -t size
```

若 Flash 沒下降，檢查相關 symbol 是否仍存在。若 RAM 沒下降，優先看 `.bss` 大戶，尤其是 `gAnimationRegistry`、`gAnimationVariants`、`game`、`SD`。
