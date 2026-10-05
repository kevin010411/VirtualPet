# 程式碼目錄與歸檔規則

專案按實際責任分成九個主要資料夾。`src` 放實作，`include` 放對應標頭；相同責任使用相同路徑名稱。每個資料夾直接放檔案，避免為單一類別建立 `application`、`domain`、`adapters` 或 `ports` 子資料夾。

```text
src/                         include/
  platform/                    platform/
  controller/                  controller/
  game/                        game/
  pet/                         pet/
  appearance/                  appearance/（含 detail/）
  animation/                   animation/
  display/                     display/
  resources/                   resources/（含 detail/）
  common/                      common/
lib/
  README                       目前沒有專案私有 library
```

| 分類 | 責任與主要檔案 | 原分類 |
| --- | --- | --- |
| `platform` | 入口、硬體組裝、板級設定、按鍵、電源及 TFT 校正驅動：`main.cpp`、`BoardConfig.h`、`ButtonInput`、`CalibratedST7735` | `platform`、`platform/hardware` |
| `controller` | 電子雞整體協調、啟動、流程、命令與遊戲接入：`Game`、`GameStartup.cpp`、`AppFlowController`、`CommandController`、`CommandExecutor`、`SystemCommandCatalog`、Status Set 工具、`MinigameController` | `presentation/application` 的流程檔案、`commands`、小遊戲接入控制 |
| `game` | 可獨立遊玩的遊戲規則、回合與勝負狀態：目前為 `GuessItemGame`，透過 `GuessItemGameHost` 使用外部播放與結果結算能力 | `minigames/guess_item` 的遊戲規則 |
| `pet` | Pet 狀態、Pet Stat/Action 規則、每日變化、數值分類／解析及存檔：`Pet`、`PetBehaviorRuntime`、`PetBehaviorRuntimeRules`、`PetStateClassifier`、`RuntimeValueResolver`、`PetStorage`、`PetSaveController` | `pet`、`pet_behavior` 的規則與執行期檔案 |
| `appearance` | 外觀選單、外觀變更與解鎖交易、Evolution 生命週期、Evolution/Outfit 查詢、SD 外觀載入：`AppearanceSelectionController`、`AppearanceChangeController`、`EvolutionController`、`AppearanceLoader`、`SdAppearanceLoader`、`RuntimeTableAppearance`、`EvolutionConditionContract.h` | `appearance/application`、`domain`、`adapters`、`ports` |
| `animation` | 播放序列、播放狀態及基礎動畫輪替：`Animation.h`、`AnimationController`、`BaseAnimationRotation` | `animation/application`、`domain` |
| `display` | 版面、播放區域、幀解碼、顯示緩衝及除錯輸出：`LayoutRenderer`、`Renderer`、`FrameDecoder`、`TftDebugDisplay`、`RenderStatsReporter` | `presentation/application/LayoutRenderer`、`presentation/adapters/rendering` |
| `resources` | SD 契約與 pack 的格式、載入、索引及查詢：`RuntimeContractLoader`、`RuntimeTableBehavior`、`RuntimeTableReader`、`RuntimeTableFile`、`AssetRuntimeContract`、`BundleReader` | `pet_behavior/domain` 的載入檔案、`shared/assets`、`shared/runtime_table` |
| `common` | Profile 設定、CRC、文字／數字工具、隨機數、共用 SD 讀取與除錯介面：`AppProfile.h`、`Crc32`、`FirmwareRandom`、`TextBuffer.h` 等 | 其餘 `shared/config`、`debug`、`integrity`、`sd`、`utils` |

## 保留的責任分工

- `controller/Game` 是電子雞的總控制模組；目前保留既有類別名稱與介面。`PetBehaviorRuntime` 和 `Pet` 處理寵物數值與原子提交。
- `game` 只放可獨立遊玩的遊戲。`controller/MinigameController` 負責將遊戲接入電子雞的動畫與行為系統；`game/GuessItemGame` 負責遊戲自身的回合、輸入與勝負。
- `controller` 按整體協調責任歸檔，不按類別名稱歸檔。`AnimationController`、`AppearanceSelectionController`、`AppearanceChangeController`、`EvolutionController`、`PetSaveController` 留在其功能模組，避免將功能內部控制集中到總控制資料夾。
- `AnimationController` 決定播放狀態；`Renderer` 持有顯示及有界緩衝；`FrameDecoder` 解碼並輸出像素；`BundleReader` 定位、檢查並讀取 pack。
- `PetStorage` 仍負責雙槽 SD 存檔；沒有搬到通用資源載入模組。
- 外觀查詢屬於 `appearance`；共用二進位表格讀取屬於 `resources`。`detail` 標頭只供其實作共享，不作為應用層入口。
- `appearance/EvolutionController` 擁有進化階段與播放失敗決策；Game 只協調檢查、播放結果與取消，不透過 getter 控制進化內部狀態。
- `common` 只放確實共用的設定、工具或介面；功能專屬檔案留在其責任資料夾。

這次重新分類沒有合併類別、增加包裝層、改變功能開關或改寫執行流程；`AppearanceLoader` 等既有介面保留。

## 新檔案與 lib 的使用

先依責任將新檔案直接放入既有九類之一。新的獨立遊戲放入 `game`；接入電子雞主流程的控制放入 `controller`。只有出現一組有獨立維護需求的檔案時才新增子資料夾；不要因為一個類別的理論分層新增目錄。內部標頭可使用 `detail` 表明適用範圍。

目前 `lib` 只有 README，沒有可重新分類的程式碼。第三方 Adafruit／SdFat 依賴由 `platformio.ini` 的 `lib_deps` 管理，PlatformIO 放在 `.pio/libdeps`；這些安裝產物不納入本專案的目錄重組。只有真正能獨立供其他專案使用、擁有明確介面及依賴的私有 library 才放入 `lib/<library-name>`。

## 驗證

2026-10-06 移動前的 `project_29` 建置基線為 Flash 57,228 B、靜態 RAM 6,788 B；Game 啟動／恢復／重置／fatal host 測試通過。

| 項目 | 移動前 | 移動後 |
| --- | ---: | ---: |
| `src` 子資料夾 | 34 | 8 |
| `include` 子資料夾 | 40 | 10 |
| C++ 實作檔 | 39 | 39 |
| 標頭與 `.def` 檔 | 53 | 53 |
| `project_29` linked Flash | 57,228 B | 57,224 B |
| `project_29` 靜態 RAM | 6,788 B | 6,788 B |

移動後 `platformio run -e project_29` 建置通過。全部 13 個既有 `run_host_test.ps1` 通過：animation_scene_playback、bundle_reader_on_access、button_cheat_input、custom_layout_export、firmware_random、game_startup、layout_media_export、pet_behavior_runtime、pet_persistence、pet_storage_sd_instance、renderer_startup_error、runtime_contract_loader、runtime_table_behavior。

92 個實作與標頭檔逐一對照移動前的 HEAD，除路徑替換及忽略 CRLF/LF 差異外，沒有其他內容變更；`git diff --check` 通過。原本沒有 host 腳本的測試目錄未另外新增或執行測試入口。所有測試引用同步更新。

Flash 的 4 B 差異只記為連結結果；不以目錄精簡推論效能提升。沒有上傳韌體、執行實機 SD／TFT／按鍵驗收或時間／SRAM 峰值量測。

### controller 與 game 分類修正

同日依電子雞總控制與獨立遊戲的責任區分，將 18 個實作／標頭檔移至 `controller`，`game` 只保留 `GuessItemGame`。目前 `src` 為 9 個子資料夾，`include` 為 11 個（含兩個 `detail`）；實作與標頭數量不變。

本次 `project_29` 建置通過，linked Flash 57,228 B、靜態 RAM 6,788 B。重新執行受路徑調整影響的 4 個 host 測試：game_startup、animation_scene_playback、custom_layout_export、runtime_table_behavior，全部通過；141 個程式、測試與工具檔案逐一比對調整前快照，只有對應路徑替換。既有類別名稱、介面與功能開關保留，未執行實機驗收。上方 13 個 host 測試及 57,224 B 為第一次目錄整理的驗證紀錄。

知識圖譜重新索引因 MCP 連線中斷未完成，查詢時仍可能顯示舊的 `game` 路徑；實際檔案與引用已更新。
