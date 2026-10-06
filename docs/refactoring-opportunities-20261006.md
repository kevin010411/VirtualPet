# 目前重構機會與可直接使用的 Prompt

分析日期：2026-10-06。依據目前工作區，包含尚未提交的 PetSession、結果介面、FirstLaunch 與 Game routing 重構；不是只看 HEAD。這份文件是分析與後續執行指令，未修改韌體，也未重新執行測試或建置。

## 結論

Game 已接近合理的電子雞總控制器。PetSession、EvolutionController、AppearanceChangeController、PetSaveController、CommandExecutor 以及 LayoutRenderer 各自已有實際責任；不建議為了縮短 Game 再增加 InputRouter、FrameCoordinator 或通用 ErrorController。

這輪應先刪除失去入口的功能、統一致命錯誤收尾，再整理資源解碼與依賴方向。不要重做已完成的 Session 抽離、FirstLaunch gate 清理、左右鍵及播放後處理去重。

| 順序 | 工作 | 收益 | 建議 |
| --- | --- | --- | --- |
| 1 | 清理無入口的物種選擇流程 | 減少狀態、陣列、查詢介面及 Game 分支 | 優先 |
| 2 | 統一致命錯誤的執行期收尾 | 讓 fatal、播放取消及後續輸入遵守同一規則 | 優先，補有意義的失敗路徑測試 |
| 3 | 分開 Runtime Table 行為與畫面解碼 | 讓畫面格式的變更集中，不牽動行為解碼 | 中優先 |
| 4 | 移除 AnimationController 的 SD 轉接入口 | 簡化不合理的依賴鏈 | 小批次即可 |
| 5 | 收斂 Startup／FirstStart 播放生命週期 | 讓 Game 不需知道序列細節與完成旗標規則 | 條件式，最後再做 |

## 1. 無入口的物種選擇流程

證據：`src/appearance/AppearanceSelectionController.cpp:48` 的 `startSpecies()` 在目前 src/include/test 搜尋只找到定義與宣告，沒有呼叫者。它仍維護 speciesOptions、speciesDefaultOutfits、索引及 selectingSpecies；`Game::OnConfirmKey()` 仍處理物種確認。`loadSpecies()` 的正式用途也來自此入口，另外有 parser 測試。

FirstLaunch 選擇 gate 已移除，但這些下游功能還在。應沿呼叫鏈清理，保留真正被 Evolution、Session、Outfit 使用的外觀查詢。不能把「不再開物種選單」解讀成「不需要 Species 二進位段落」。

`ENABLE_OUTFIT_CHOOSE_ANIMATION` 還出現在 Web Profile Resolver 的設定清單與韌體 feature 檢查，因此刪除選單程式不等於可以同批刪除跨層旗標或二進位角色。

## 2. fatal 收尾仍有多個版本

證據：`src/controller/Game.cpp:515` 的 `enterRuntimeFatal()` 設定 RuntimeLoadState、取消動畫與 Evolution、設定 flow 並顯示錯誤。但 loop 開頭、Startup 缺少動畫／排隊失敗、FirstStart 播放失敗、handlePlaybackResult，以及 GameStartup 仍有直接呼叫 `flow.enterFatalError()` 的路徑。

這些路徑的取消與狀態更新不同。另需驗證：方向鍵與確認鍵在判斷 Command 前，會先派送 active appearance selection；redrawAllNow 也會推進播放。現有 initialized 在 runtime fatal 後不一定變成 false，因此「已有 fatal flow」本身不是所有公開入口的共同阻擋條件。這是需要測試確定的風險，這次未做執行期缺陷驗證。

啟動準備發生在顯示初始化之前，不能把所有錯誤路徑盲目改成會立即顯示的 helper。先区分錯誤狀態收尾與顯示時機，保留 prepare／finish 的兩階段協定。

## 3. RuntimeTableBehavior 同時擁有畫面解碼與完整載入流程

證據：`src/resources/RuntimeTableBehavior.cpp` 同時包含 decodeRuntimeTableBehavior、compiledFeaturesAccept、decodeScreenBlocks（484）、decodeRuntimePresentation（587）及 loadCompleteRuntimeTable（664）。畫面矩形、區塊規則、圖案 frame 與 SystemRoles 的知識集中在行為檔案裡。

適合形成內部的 presentation 解碼 module，仍共用 RuntimeTableReader／RuntimeTableFile 與同一份 config。完整載入順序留在一個位置，不新增多次開檔，也不必新增 decoder class。分拆的價值是讓畫面規則與行為規則各自在本地維護，不是讓檔案字數平均。

## 4. Game 經動畫控制器取得 SD

證據：Game::configureActiveAppearance 與 prepare_game 呼叫 animations->sdCard()；`src/animation/AnimationController.cpp:423` 只轉呼叫 renderer.sdCard()。這個方法沒有隱藏任何播放責任，讓契約載入必須繞過動畫 module。

第一個低成本選擇是 Game 使用已持有的 Renderer 取得 SD，先刪掉 AnimationController 的轉接方法；這只能消除一層不必要依賴。若希望 SD 直接由 platform 注入，應另比較建構介面、固定參照成本及測試 fixture 變更，不需為這兩個呼叫建立新 storage facade。

## 5. Startup／FirstStart 是剩餘可抽離的生命週期

證據：`src/controller/Game.cpp:524` 的 beginStartupAnimation 組合三段序列、決定 duration、檢查必需動畫；594 的 completeFirstStartIfReady 管理 pending、完成旗標與存檔失敗回復。Game 仍持有 pendingFirstStartCompletion。

若啟動功能持續演進，可抽成一個小介面的生命週期 module。若短期沒有變更需求，現有私有函式可以保留。目前 project_29 關閉 Startup／FirstStart，且已有帶 -EnableFirstStartAnimation 的 host 測試；不能因預設 profile 關閉就刪除支援，也不能只驗證關閉的 profile。

## 共通執行原則

以下每個 Prompt 都可獨立貼入新任務。一次只執行一個批次；先核對目前程式，避免重做其他任務已完成的變更。

- 工作區：`E:\C++\virtualPet\main\code`。保留既有未提交變更，不自行 stage、commit、燒錄或修改使用者 profile。
- 遵守 AGENTS.md；先用 codebase-memory-mcp，圖譜不足時以原始碼核對。本次重新索引後仍漏掉部分新 class，不把圖譜零結果當作無呼叫者的唯一證據。
- 涉及跨層語意時，先讀 `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`，必要時追讀它指向的二進位規格。不要在本 repo 複製規格。
- 不改 runtime 格式、schema fingerprint、存檔版本、Action 原子提交與動畫失敗保留已提交值、Evolution 時序、首次錯誤資源及需要重啟的規則。
- 不增加第二份大型 PetBehaviorConfig、不新增逐幀配置或無界容器；不以純轉呼叫 class 代替責任整理。
- 執行受影響 host 測試與目標建置。以同一設定記錄修改前後 linked Flash／靜態 RAM；不把它們當成 SRAM 峰值或速度證據。失敗與未驗證項目分開報告。
- 每次更新韌體專屬架構說明與一份精簡驗證紀錄；不重寫無關歷史文件。

## Prompt 1：清理無入口的物種選擇

```text
請在 E:\C++\virtualPet\main\code 執行「清理無入口的物種選擇流程」這一個重構批次。
先閱讀 AGENTS.md、docs/refactoring-opportunities-20261006.md 的共通原則及第 1 項，依目前原始碼重新確認呼叫者。

目標：AppearanceSelectionController 收斂為現行可達的 Outfit 選擇功能，移除已失去入口的物種選單責任。
1. 追查 startSpecies、onConfirmSpecies、isSelectingSpecies、loadSelectedSpeciesPreview、playSelectedChooseAnimation 及其狀態和陣列。
2. 證明 src/include、測試、所有支援的編譯條件沒有正式入口後，移除不可達分支，包括 Game::OnConfirmKey 的物種選單分流。
3. 追查 AppearanceLoader::loadSpecies、SdAppearanceLoader 及下游 query；只移除專供已刪選單的操作。保留初始外觀、Evolution、Outfit range、必要 Species 段落解碼與格式驗證。
4. 不自行移除 Web Profile Resolver 仍輸出的旗標、runtime feature bits、SystemRoles 或 FirstStart。發現需要跨 repo 語意變更時，列為獨立後續工作。
5. 保留 Outfit 選擇、鎖定預覽、消耗解鎖及失敗保留已發生變更的語意；不要為精簡另新增 Controller。

驗收：無殘留的不可達物種選單狀態；既有 runtime_table_behavior 與 game_startup 測試通過；對 Outfit 開啟組態執行編譯及有意義的操作回歸，不能只測 project_29 關閉功能的路徑。執行同設定目標建置並報告容量差異。若既有 host 無法涵蓋 Outfit，新增最小的公開介面測試，不寫 private helper 結構測試。
只做本批，保留其他工作區變更，不 commit 或燒錄。最後用中文列出刪除責任、保留契約、驗證與未驗證項目。
```

## Prompt 2：統一致命錯誤收尾

```text
請在 E:\C++\virtualPet\main\code 執行「Game 致命錯誤收尾收斂」這一個重構批次。
先閱讀 AGENTS.md、docs/refactoring-opportunities-20261006.md 的共通原則及第 2 項，重新核對現況。

目標：所有執行期致命錯誤遵守一致的停止規則，並保留啟動兩階段的顯示時機。
1. 列出 Game／GameStartup 每個 enterFatalError 呼叫的觸發、是否已初始化 TFT、動畫與 Evolution 如何取消、pending FirstStart 如何處理、資源錯誤如何顯示。
2. 在既有 Game 私有操作內收斂真正相同的收尾；不要新增通用 ErrorController，不要把不同失敗一律改成 fatal。
3. 特別處理 fatal 後的 OnLeftKey／OnRightKey／OnConfirmKey、redrawAllNow、Battery 入口與 completePlaybackTick：不能再提交照護、切換外觀、完成 FirstStart 存檔或重新啟動播放。用公開入口測試證實，避免只驗證 flow enum。
4. 保留 prepare_game 在 TFT 初始化前只準備、finish_setup_game 才顯示啟動錯誤的協定。不要為重用會立即繪圖的 helper 破壞平台順序。
5. 保留第一錯誤資源；Evolution 非致命失敗與目標提交後的既有完成政策、Action 已提交值及一般存檔失敗語意不改變。
6. 若測試證明現有行為違反 fail-closed，明確區分這個修正與純重構，不默默混入其他政策修改。

驗收：game_startup 預設及 -EnableFirstStartAnimation 均通過；renderer_startup_error 通過；新增涵蓋 active Outfit 選單後進入 fatal、fatal 後重繪／按鍵、FirstStart 失敗的必要回歸案例。以同設定完成 project_29 建置與容量比較。保留未提交變更，不 commit 或燒錄，中文報告實際執行與未驗證項目。
```

## Prompt 3：整理 Runtime Table 解碼責任

```text
請在 E:\C++\virtualPet\main\code 執行「Runtime Table 行為與畫面解碼責任整理」這一個批次。
先閱讀 AGENTS.md、docs/refactoring-opportunities-20261006.md 的共通原則及第 3 項，以及權威規格。

目標：把 SCREEN_BLOCKS／SCREEN_RULES、SystemRoles 與畫面素材解析的知識，從 RuntimeTableBehavior 的行為解碼實作分離成 resources 下的內部 module。
1. 使用現有 RuntimeTableReader／RuntimeTableFile，設計一個小型內部 decode 入口；優先普通函式及 detail 標頭，不為分檔新增 class 或 public facade。
2. 行為解碼保有 Pet Stats／Actions／Status 等現有責任；共用 compiled feature 檢查放在符合其用途的位置，不強行歸給畫面。
3. 明確指定完整載入順序的唯一擁有者。保留初始外觀解析、行為、畫面、已用素材驗證與 appearance sections 的現有順序及首錯資源。
4. 保留 SD 路徑直接解碼到呼叫者 config 的低 SRAM 策略，不引入第二份大型 config、額外開檔、常駐快取或新的格式兼容。
5. 保留畫面 bounds、容量、reference、區塊與 frame 檢查；本批不刪驗證、不調整資料格式、不拆成每個 record 一個 module。
6. 更新受影響 host 的 source 清單，以既有對外 parse/load 介面驗證，不為私有拆分重寫全部測試。

驗收：runtime_table_behavior、runtime_contract_loader、custom_layout_export、layout_media_export 的受影響回歸，以及同設定 project_29 建置；比對失敗順序與 linked Flash／靜態 RAM。只做此批，不 commit 或燒錄，中文報告。
```

## Prompt 4：移除動畫模組的 SD 轉接責任

```text
請在 E:\C++\virtualPet\main\code 執行「移除 AnimationController::sdCard 轉接入口」的小批次重構。
先閱讀 AGENTS.md、docs/refactoring-opportunities-20261006.md 共通原則及第 4 項，重新確認所有呼叫者。

目標：Game 的契約載入不再經過動畫排程 module 取得 SD。
優先採最小變更：Game 已持有 Renderer，改用 renderer.sdCard()，刪除 AnimationController::sdCard 宣告與純轉呼叫實作，並移除確定不再需要的標頭依賴。
不要新增 StorageProvider／ResourceService，不改 Renderer 或 RuntimeContractLoader 的生命週期，不把 SD 改成全域 singleton，也不順帶重構整個物件建構圖。
如果 live code 已採直接注入 SD 或已移除此入口，不要退回上述設計，直接確認完成並說明。

驗收：無正式呼叫者再透過 AnimationController 取得 SD；game_startup 與 animation_scene_playback 受影響測試及 project_29 建置通過，核對同設定尺寸。保留未提交變更，不 commit 或燒錄，中文報告。說明本批只消除一層依賴，沒有宣稱完全解耦 Renderer 與資源。
```

## Prompt 5：條件式抽離 Startup 生命週期

```text
請在 E:\C++\virtualPet\main\code 評估並執行「Startup／FirstStart 生命週期收斂」，只做這個批次。
先閱讀 AGENTS.md、docs/refactoring-opportunities-20261006.md 共通原則及第 5 項；先確認 fatal 收尾已有一致規則。

目標：若能形成小而完整的介面，讓同一 module 擁有 Intro／FirstStart／Start 序列建立、完成觀察、取消及 FirstStart 完成存檔規則。
1. 檢查 beginStartupAnimation、completeFirstStartIfReady、pendingFirstStartCompletion 及 Battery／reset／fatal 中斷路徑。
2. 外部只需要開始、接收播放結果、取消等少數生命週期操作，並回報明確結果；Game 保有全域模式、fatal 決策及 tick 時序。
3. 新 module 不持有整個 Game，不管理輸入、LayoutRenderer、PetSession、Evolution 或小遊戲，也不透過多個 host callbacks 把細節又交回 Game。
4. 保留播放順序、duration、必要 FirstStart 缺失失敗、成功後存檔、存檔失敗清除完成旗標、重置後重播的語意；不引入 FirstLaunch 選單或改變存檔格式。
5. 優先固定生命週期配置；若抽離只增加轉呼叫與參照而沒有隱藏實質規則，保留現有私有函式，記錄不採用理由，不勉強新增 class。

驗收：game_startup 預設與 -EnableFirstStartAnimation、animation_scene_playback；涵蓋缺少必要動畫、排隊失敗、FirstStart 播放失敗、完成存檔失敗、重置與中斷。執行 project_29 及啟用 Startup／FirstStart 的適當編譯檢查，說明 host、target build 與設備驗證的差別。記錄容量成本，不 commit 或燒錄，中文報告。
```

## 目前先保留的設計

- Game 的總體組裝、輸入派送、每日 tick、模式切換、跨 module 結果協調，以及 activateLoadedAppearance 的設定分配。
- PetSession 的 prepare 結果與 Game 的 initialized 分別表示資料準備與可執行狀態，不能僅因都是狀態就合併。
- AppearanceLoader 的查詢責任與既有測試替身，不因 virtual 方法多就拆成多個 port。
- Renderer／BundleReader／FrameDecoder 的串流路徑、快取與緩衝尺寸；沒有實機量測，不據此提出效能重構。
- Game 可在獨立的機械更名批次改為 PetController，以符合 controller 與獨立遊戲的用語；更名本身不會收斂責任，優先度低於上述項目。

目前 game_startup host 已提供 FirstStart 開啟變體，但 Outfit／Guess 路由仍關閉。因此後續改動這些分支時，需要對應的開啟組態驗證，不能把預設測試通過當作全部功能都已覆盖。本次未執行這些測試；既有 .agent 驗證文件屬之前批次的證據。
