# 韌體重構進度與實作計畫

更新：2026-09-24。範圍：`E:\C++\virtualPet\main\code`，`project_12`，基準 HEAD `d141db0`。這是工作追蹤文件；基準與交接為 `67cdb74`，啟動載入重構為 `03704da`，Flash 亂數優化為 `975fe72`。跨層 Pet Stat、Behavior、Status、Evolution、持久化與 SD 語意以 `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md` 為準；此處只記韌體工作與驗證。

## 目前進度

| 項目 | 狀態 | 證據與限制 |
| --- | --- | --- |
| 現況架構分析 | 已完成唯讀分析 | 2026-09-24 交接文件 `virtualpet-firmware-refactor-handoff-2026-09-24.md`；後續決策仍須對照現行原始碼 |
| `project_12` 建置尺寸基準 | 已完成 | 清理後完整重建成功；Flash 61,496 B、靜態 RAM 6,840 B、BIN 61,792 B，詳見 `docs/project_12_measurement_baseline_2026-09-24.md` |
| 實機效能基準 | 依使用者指示跳過 | ST-Link 可連線，但無可讀取的 SD 報表或序列介面；啟動、逐幀、heap、stack 均未量測。後續不可聲稱效能改善 |
| 啟動 `runtime.bin` 讀取路徑 | 已完成靜態追查 | 初始外觀設定完成前至少五次開檔；計數不是時間量測 |
| 啟動 manifest、初始外觀與必要外觀段落讀取收斂 | 已實作並完成 host 驗證，待實機驗證 | 首次啟動沿用已讀取 manifest；同一次完整表讀取取得初始外觀、行為設定及必要外觀段落；後續外觀變更仍重新讀取 |
| Flash：亂數模組收斂 | 已實作並完成建置與 host 驗證，待實機驗證 | 以共用無配置的 `FirmwareRandom` 取代韌體對 Arduino `random/randomSeed` 的呼叫，保留原有選擇範圍與類比輸入種子；`project_12` Flash 61,628 → 57,992 B，靜態 RAM 6,840 → 6,828 B；三項既有第三方依賴未移除 |
| Flash：錯誤資源字串複製 | 已實作並完成建置與 host 驗證 | 共用只複製到字串結尾的有界函式，移除連結後的 `strncpy`；`project_12` Flash 57,992 → 57,948 B，靜態 RAM 維持 6,828 B。BundleReader 測試資料改用現行 pack 版本後通過 |
| Flash：初始外觀責任收斂 | 已實作並完成建置及相關 host 測試，待 Game/實機驗證 | 啟動時已驗證的初始 species/outfit 成為 Game 當次工作階段的初始值；新狀態與重置直接使用，移除 `AppearanceLoader::findInitialAppearance` 的重讀介面；Flash 57,948 → 57,860 B，靜態 RAM 維持 6,828 B |
| 架構方案與過時指引清理（第一步） | 已更新文件，未修改韌體程式碼 | `docs/architecture.md` 明確分開現況與 Proposed Architecture，列出逐步移除條件；建置、profile、size 文件改依目前 `project_12`；舊 `/index/` 格式標為歷史資料，Renderer、SD 範例及動畫播放文件改指向現行 `.data` pack |
| 載入責任：啟動契約介面（第二步） | 已收斂一條流程並完成 host/建置驗證，待實機驗證 | `Game` 不再先讀 manifest 或傳遞已驗證 manifest；`RuntimeContractLoader` 負責啟動兩次讀取、完整表驗證及錯誤資源。一般外觀切換仍重新讀 manifest 與完整表；Flash 57,860 → 57,892 B，靜態 RAM 維持 6,828 B |
| Game 協調：啟動與外觀狀態（第四步） | 程式與 host 整合驗證完成，待實機 SRAM 峰值與 SD 驗證 | 新狀態沿用已啟用的外觀設定；不同外觀恢復、重置與後續切換仍重新載入。載入布林旗標收斂為三態，release fatal 畫面優先顯示第一個資源。Flash 57,884 → 57,856 B，靜態 RAM 6,828 → 6,824 B |

## 重構前：啟動讀取現況

`Game::prepare_game` 依序呼叫 `loadRuntimeManifest`、`SdAppearanceLoader::findInitialAppearance`、`Game::configureActiveAppearance`。最後一項透過 `loadRuntimeContract` 再讀 manifest，接著 `loadCompleteRuntimeTable` 讀完整表，並以 `SdAppearanceLoader::validateRuntimeContracts` 再讀外觀。這些步驟各自開啟 `/runtime.bin`，所以在這段路徑內至少開檔五次。程式位置：`src/presentation/application/Game.cpp`、`src/pet_behavior/domain/RuntimeContractLoader.cpp`、`src/pet_behavior/domain/RuntimeTableBehavior.cpp`、`src/appearance/adapters/SdAppearanceLoader.cpp`。

完整表讀取會檢查 envelope 與已取得 manifest 的檔案大小、bundle ID、schema fingerprint、file CRC，再解碼行為、顯示與外觀。載入失敗可能回報 `runtime.bin` 或 BundleReader 記錄的第一個資源；外觀驗證另有第一個資源錯誤。`PetBehaviorConfig` 約 6 KiB，目前直接清空並解碼到呼叫者持有的緩衝區，以免建立第二份大型 stack candidate。後續改動必須保留這些驗證、錯誤順序、低 SRAM 用量、失敗後進入 fatal/reboot 的語意。

首次啟動的儲存狀態可能恢復不同 species/outfit，此時 `prepare_game` 會再次呼叫 `configureActiveAppearance`。日後切換外觀也共用這條路徑。因此不可把啟動時取得的 manifest 無條件當成整個工作階段永久有效，也不可把首次與後續外觀載入的驗證需求混為一談。

## 已實作：共用 manifest 與完整表讀取

首輪載入重構時，`prepare_game` 先讀取 manifest，再呼叫 `loadInitialRuntimeContract`。後者以已讀取的 manifest 開啟完整表一次，先解析初始 species/outfit，再解碼該外觀的行為與顯示設定；同一讀取仍執行 envelope 比對、bundle 引用、idle animation 及必要外觀段落檢查。原本 `SdAppearanceLoader::validateRuntimeContracts` 的檢查只驗證 Appearance、Species、Outfits、OutfitUnlocks，以及啟用演化時的 Evolutions 段落是否存在，已移到完整表讀取後段；不再為此重開 `/runtime.bin`。設定直接寫入 `Game` 持有的 `PetBehaviorConfig`，沒有第二份約 6 KiB 暫存。初始外觀解析失敗與其後的設定失敗仍可區分；恢復不同外觀及一般外觀切換仍重新讀取 manifest 與完整表，且執行相同段落檢查。第二步再將啟動 manifest 讀取移入載入器，見下文。

依原始碼路徑，初始外觀設定完成前的 `/runtime.bin` 開檔由重構前至少五次降為兩次：manifest、初始外觀與完整設定合併讀取。首次啟動後的 `enterSpecies` 目前仍重讀 manifest 與完整表；解鎖與存檔恢復也可能繼續開檔，所以此計數不代表整個啟動流程總開檔數，更非時間量測。

2026-09-24 驗證：`pio run -e project_12` 成功，Flash 61,628 B（較基準 +132 B，較上一步 -124 B）、靜態 RAM 6,840 B（持平）、BIN 61,924 B（較基準 +132 B）；`test/runtime_table_behavior/run_host_test.ps1`、`test/runtime_contract_loader/run_host_test.ps1` 通過；`git diff --check` 通過。Runtime Table host 測試使用完整外觀 fixture，驗證同一次開檔回傳初始外觀與對應設定、後續外觀同樣單次開檔、缺少必要外觀段落會拒絕，以及 manifest 不符時停止解析。載入器 host 測試以測試替身驗證錯誤階段、資源傳遞及後續外觀重新載入。未直接執行 `Game::prepare_game` 或實機 SD 讀取，實機執行依使用者指示跳過。

## Flash Optimization：已實作與量測

`project_12` 的連結 ELF 顯示，Arduino `random/randomSeed` 會帶入 newlib `rand/srand`；後者的配置失敗斷言再帶入 `__assert_func`、`fprintf` 與 stdio。實際選隨機 Status、Action 結果、動畫版本與 Guess Game 只需要有界整數，現在共用 `FirmwareRandom` 的一個 32 位元狀態與 xorshift32。啟動仍使用原本的 `analogRead(0)` 作為種子；零種子保持現有狀態，零上界回傳零；範圍與單次抽樣次數維持不變，但相同種子下的抽樣序列不同於 newlib `rand`。所有既有第三方庫與原本使用的圖形、SD 功能保持在建置中。

同一工作樹、同一 `project_12` 設定的 A/B 連結比較：更動前 Flash 61,628 B、靜態 RAM 6,840 B、BIN 61,924 B；更動後 Flash 57,992 B、靜態 RAM 6,828 B、BIN 58,288 B。Flash 與 BIN 各減少 3,636 B；相對乾淨 HEAD 基準 Flash 減少 3,504 B。更動後 `nm` 不再出現 `rand/srand`、`__assert_func`、`fprintf`、`_vfiprintf_r`。`test/firmware_random/run_host_test.ps1` 驗證範圍、固定種子重現、零種子與零上界；`test/animation_scene_playback/run_host_test.ps1`、Runtime Table 與 Runtime Contract Loader host 測試通過，PlatformIO 建置成功。未在實機驗證亂數分布、遊戲互動或效能。

2026-09-24 後續 Flash 拆解：保留 Status 等級換算原本的除法，因使用者指出減法迴圈太慢，該 852 B 節省未採用。`project_12` LTO ELF 的下一批大符號有 `loadFromManifest`、`decodeRuntimeTableAppearance` 與 `Renderer::ShowAnimationFrame`；動畫逐幀路徑沒有為了尺寸加入額外呼叫或迴圈。錯誤資源名稱的四個複製點改為共用有界字串複製，避免 `strncpy` 在短字串後填滿緩衝區；只在錯誤記錄路徑執行。`project_12` Flash 57,948 B、靜態 RAM 6,828 B、BIN 58,244 B，較改動前 Flash/BIN 各減 44 B；三項既有第三方依賴仍在建置中。Runtime Contract Loader host 測試包含短緩衝區截斷與結尾字元案例，Renderer startup error host 測試也通過。`test/bundle_reader_on_access/run_host_test.ps1` 在 `reader.openFrame(validAddress, frame)` 斷言失敗；以未修改的 HEAD `BundleReader.cpp` 重編同一測試也在同一行失敗，故此測試尚不能驗證本次 BundleReader 變更。未進行實機效能量測。

2026-09-24 本輪熱點拆解：上述 BundleReader 測試失敗來自 fixture 在兩處硬編 pack 版本 `1`，目前 `AssetData::kVersion` 為 `2`，因此開檔時被判為 `InvalidPack`。fixture 改用共用版本常數後，`test/bundle_reader_on_access/run_host_test.ps1` 通過，先前的錯誤資源變更現有 host 覆蓋。另以 57,948 B 為 A/B 基線，測試簡化 Runtime Table 目錄邊界加法，Flash 為 57,952 B（+4 B）；測試阻止 `Game::OnConfirmKey` 內聯，Flash 為 57,980 B（+32 B）。兩項均未保留。`RuntimeTableBehavior.cpp` 的 `RuntimeTable::find` 已是連結後共用的 36 B 函式，而非十次完整內聯；因此單純抽出搜尋函式不是有效節省。LTO 符號尺寸仍為 `loadFromManifest` 約 3,292 B、`decodeRuntimeTableAppearance` 約 2,392 B、`onConfirmButton` 約 1,940 B。未量測實機執行時間。

2026-09-24 再次 A/B 拆解：阻止 `decodeActions` 與 `decodeRuntimeTableBehavior` 內聯合計只省 24 B，卻多兩次啟動載入呼叫；把 `AppearanceQueryKind` 的段落查找延後，Flash 增加 80 B。兩者均還原。曾測試以固定緩衝區直接組 BundleReader pack 路徑，Flash 減少 28 B；使用者明確表示此類局部省位元組優化不符合目標，實作與專用測試已撤回。後續以模組責任、重複流程與介面收斂為主要依據，尺寸仍以 A/B 建置確認。

2026-09-24 架構優化：`prepare_game` 原本已由 `loadInitialRuntimeContract` 取得並驗證初始外觀，但 `loadInitialPetState` 在新狀態路徑又透過 `AppearanceLoader::findInitialAppearance` 重讀 Runtime Table；`resetPet` 也經此介面重讀。Game 現在保存本次啟動已驗證的 species/outfit，初始狀態與工作階段內重置沿用，並從 `AppearanceLoader` 介面及 SD adapter 移除該專用方法。這把初始外觀歸到啟動契約的結果，不再由狀態初始化自行取得；恢復存檔的外觀預覽驗證與必要時的其他外觀重載仍維持。`project_12` 的 A/B 連結尺寸 Flash 57,948 → 57,860 B（-88 B）、靜態 RAM 6,828 B（不變）；Runtime Table、Runtime Contract Loader、BundleReader host 測試通過。未直接執行 `Game::prepare_game/resetPet` 的 host 整合測試，也未做實機 SD 或時間量測；重置時的初始選擇現在固定為本次啟動已驗證的值，符合需重新開機才能載入更換內容的契約。

## 第二步：載入責任盤點與首條流程收斂

現行 `RuntimeContractLoader` 啟動流程先開 `/runtime.bin` 讀 manifest，再開一次讀完整表並解析初始外觀與設定。一般外觀切換也先讀 manifest、再讀完整表，且完整表會對照 manifest 驗證 envelope、bundle 與必要段落。這次把 manifest 開檔移入 `loadInitialRuntimeContract`，移除 `loadRuntimeContract` 的已驗證 manifest 可選參數；`Game` 不再協調兩個載入結果。初始外觀解析失敗與後續設定失敗仍由 `initialAppearanceResolved` 區分，錯誤資源由載入器回傳。啟動與切換的開檔次數未變，未宣稱速度改善。

外觀查詢目前由 `SdAppearanceLoader` 保存載入後的 manifest 與 BundleReader；Evolution、species、outfits、preview、unlock 等各次查詢都經 `loadRuntimeTableAppearanceQuery` 重新開表並比對 manifest，adapter 再記錄第一個錯誤資源。這些查詢沒有在本批改動；選單的載入與錯誤順序需在下一次收斂前逐一驗證，不能共用未證明新鮮的表格快取。

同一 `project_12` 設定的 A/B 建置：Flash 57,860 → 57,892 B（+32 B）、靜態 RAM 6,828 → 6,828 B。`test/runtime_contract_loader/run_host_test.ps1` 驗證啟動只讀一次 manifest、初始外觀與後續外觀錯誤路由，以及一般切換重新讀 manifest；`test/runtime_table_behavior/run_host_test.ps1` 與 `pio run -e project_12` 通過。未直接執行 `Game::prepare_game` 或實機 SD 讀取。

## 第三步：過薄介面與舊相容入口

依現行呼叫者與 Web `backend/services/feature_sets/profile_resolver.py` 的旗標映射核對：`APP_PROFILE_*` 舊客戶巨集沒有程式使用；Profile Resolver 不輸出 `ENABLE_COMMAND_SPECIES`，正式建置也未設定它。已移除這個直接 Species 命令的編譯分支、CommandHost/Executor 轉呼叫及 Game 的結果分支；Evolution 與首次啟動選擇仍保留。`RuntimeSystemCommandId::ChangeSpecies` 與 `APP_COMMAND_CHANGE_SPECIES` 的數值保留作為 tombstone，不會在命令目錄接受 `change_species`。只有測試與離線 SD 檢查器使用的 `loadRuntimeTableInitialAppearance` 重複開檔入口已移除；離線檢查器改用完整表讀取交付的初始外觀。`AppearanceLoader` 仍負責 manifest、BundleReader、第一錯誤資源與可替換呼叫介面；`configureRuntimeContract` 改為必須實作，避免無聲略過載入設定。`validateRuntimeTableAppearance` 保留供離線檢查及缺少必要段落的 host 測試使用。

同一 `project_12` 設定的 A/B 建置：Flash 57,892 → 57,884 B（-8 B）、靜態 RAM 6,828 → 6,828 B。`test/runtime_table_behavior/run_host_test.ps1`、`test/animation_scene_playback/run_host_test.ps1`、`test/runtime_contract_loader/run_host_test.ps1` 及 `pio run -e project_12` 通過；離線 `sd_card_runtime` 工具已做 C++ 語法編譯，未以實際 SD bundle 執行。未進行實機 SD、時間或 SRAM 峰值量測。

## 第四步：Game 啟動與外觀協調首批

`prepare_game` 在 `loadInitialRuntimeContract` 後已啟用已驗證的初始外觀。新狀態的 `loadInitialPetState` 也設定相同 species/outfit；過去再呼叫 `enterSpecies`，因而重新讀 manifest 與完整 Runtime Table。現在 `enterSpecies` 保留重載責任，另將 Pet 外觀暫存、解鎖與一次存檔放在 `commitSpeciesAppearance`；新狀態只走後半段。存檔恢復仍先預覽檢查，與初始外觀不同時重新載入；重置及日後切換維持 `enterSpecies` 重新驗證。`Game` 原本兩個互斥的載入旗標改為一個三態值，避免 Ready/Failed 同時為真。

同一 `project_12` 設定的 A/B 建置：Flash 57,884 → 57,832 B（-52 B）、靜態 RAM 6,828 → 6,824 B（-4 B）。Runtime Contract Loader、Runtime Table、Animation Scene host 測試及建置通過；`git diff --check` 通過。這些 host 測試沒有直接執行 `Game::prepare_game`、恢復、重置或 fatal 分支，因此第四步的整合驗證仍未完成。新狀態省掉一次 manifest 與一次完整表開檔是原始碼路徑推論，未量測 SD 次數或時間。未做實機驗證。

後續加入 `test/game_startup/run_host_test.ps1`，直接執行 `Game::prepare_game`、`finish_setup_game` 與 `resetPet`，使用 SD 載入、存檔、Renderer 的 host 替身驗證新狀態、同/不同外觀恢復、預覽失敗與 fingerprint 不符後回到新狀態、重置，以及載入與存檔失敗後禁止互動。測試揭露 release fatal 畫面忽略已記錄的第一資源，現已與 debug 路徑一致，優先顯示該資源，否則顯示 `runtime.bin`。Game、Renderer startup error、Runtime Contract Loader host 測試及 `project_12` 建置通過；最終 Flash 57,856 B（較本步前 -28 B）、靜態 RAM 6,824 B（-4 B）。實際 SD 開檔次數、實機時間與 SRAM 峰值仍未量測。

## 進化查詢介面收斂

2026-09-27：`AppearanceLoader` 與 `PetActionController` 的進化查詢改為 `Found`、`NoTarget`、`LoadFailed` 三態；Game 不再用 `lastContractLoadSucceeded()` 旁路判斷。載入失敗會保留受影響資源、進入 fatal，且同一輪不再繼續更新 Pet；沒有目標及目標仍為目前 species 都維持不進化。`SdAppearanceLoader` 移除只服務此旁路查詢的布林狀態，外觀查詢仍每次讀取並驗證 `/runtime.bin`，進化條件不變。

`test/game_startup/run_host_test.ps1` 覆蓋三態、同 species 目標、正常 tick 與 cheat 觸發的失敗路徑；`test/runtime_table_behavior/run_host_test.ps1` 和 `project_12` 建置通過。相對本批前 Flash 57,856 → 57,856 B（不變），靜態 RAM 6,824 → 6,820 B（-4 B）。沒有實機 SD、時間或 SRAM 峰值量測。

## 外觀查詢內部路徑收斂

完整契約載入與外觀查詢改用同一個內部 `RuntimeTableFile` 持有 SD 檔案、Source context 與已對照 manifest 的表格；物件離開查詢範圍即關檔。`SdAppearanceLoader::recordQueryResult` 統一處理六種外觀查詢的成功狀態與錯誤資源。對外 `AppearanceLoader` 操作、每次查詢重新開檔驗證、第一錯誤資源及無進化目標語意維持；沒有新增常駐快取或第二份 `PetBehaviorConfig`。

Runtime Table host 測試直接執行 SD adapter，確認 species、outfit、preview、unlock 查詢各開檔一次，manifest 不符時回報 `runtime`；Game startup 與 Runtime Contract Loader host 測試、`project_12` 建置通過。相對本批前 Flash 57,856 → 57,776 B（-80 B），靜態 RAM 維持 6,820 B。未量測實機 SD 時間或 SRAM 峰值，不宣稱速度改善。

## 後續擬議實作順序

2026-09-27 外觀 Outfit 查詢首批重構：Web Runtime Table v7 匯出已在 `SPECIES` 記錄提供 `first_outfit` 與 `outfit_count`，並依 Species/Outfit slot 排序，故本批不改後端輸出格式。韌體共用有界的 Species Outfit 範圍讀取；Outfit 列表只讀該物種範圍，預覽及消耗解鎖直接定位指定 Outfit，且保留記錄身分及越界拒絕。`runtime_table_behavior` host 測試新增第二物種、越界範圍及消耗解鎖情境；`game_startup`、`runtime_contract_loader` host 測試與 `project_12` 建置通過。連結結果 Flash 57,968 B、靜態 RAM 6,820 B。這是原始碼層面的讀取次數收斂，未量測實機 SD 時間；本批也未拆分整個外觀解碼函式。

2026-09-27 外觀解碼第二批：`decodeRuntimeTableAppearance` 僅保留共同的 Appearance feature、Asset/Animation section 檢查及查詢分派；初始外觀、Species、Outfit 列表／預覽、一般／消耗解鎖及 Evolution 的解碼規則各移至同檔案的私有函式。沒有新增對外介面、常駐表格或配置；查詢仍由 `RuntimeTableFile` 每次重新開檔與 manifest 比對。Runtime Table、Game startup、Runtime Contract Loader host 測試及 `project_12` 建置通過；連結 Flash 57,992 B（較首批 +24 B）、靜態 RAM 6,820 B（持平）。沒有實機 SD 時間或 SRAM 峰值量測。

2026-09-27 外觀查詢介面第三批：移除私有 `AppearanceQuery` 萬用欄位與 kind 分派，各查詢改由具體參數呼叫對應解碼函式；共用的 Appearance feature、Asset/Animation section 檢查與必要外觀段落檢查仍集中。`AppearanceLoader` adapter 與離線 SD 檢查器所用的表格查詢，若不解析動畫，就不再傳入 `BundleReader`。`RuntimeTableFile` 仍持有每次開檔、manifest 比對與離開作用域時關檔；`loadCompleteRuntimeTable` 的初始外觀、行為設定、idle 參照、必要段落驗證順序維持。Runtime Table、Game startup、Runtime Contract Loader host 測試及 `project_12` 建置通過；離線 `sd_card_runtime` 入口以 host stub 做 C++ 語法編譯。連結 Flash 57,896 B（較第二批 -96 B）、靜態 RAM 6,820 B（持平）。外觀規則尚在 `RuntimeTableBehavior.cpp`，搬到 `appearance/` 前須先讓共用的 bounded table reader 有小而明確的內部介面，避免把底層讀表細節全部公開。

架構清理以 `docs/architecture.md` 的 Proposed Architecture 與逐步完成條件為準。第一至第三步已完成；第四步程式與 host 整合驗證完成，實機 SRAM 峰值與 SD 尚待驗證。因無法建立實機效能基線，第五步先完成 Evolution 與外觀查詢的靜態責任收斂；逐幀熱路徑和 Renderer 容量改動尚未開始。以下為後續候選。

1. **以架構角度繼續 Flash 優化。** 初始外觀的重複查詢、Evolution 三態結果及其餘外觀查詢的開檔與錯誤處理已收斂。後續若拆分 `RuntimeTableBehavior` 的行為與外觀解碼實作，須先盤點共用的有界讀取、動畫參照與載入順序，避免另建轉呼叫層。不得再以 pack 路徑字串組裝、內聯標註等局部省位元組變動作為優化方向；不可藉移除目前使用中的第三方庫節省 Flash，也不採用 Status 減法迴圈。
2. **Game 啟動後續驗證。** host 已覆蓋協調與首錯資源；真實缺檔/損毀的完整 SD bundle、顯示與實機記憶體峰值仍需另驗證。
3. **首次啟動重複載入。** 新狀態已沿用啟動設定，Game host 測試確認不呼叫一般外觀重載；實際 SD 開檔次數仍待實機觀測。
4. **其他效能候選。** Renderer 行緩衝、每幀 pack 存取與一次性配置均未實作；使用者已跳過實機效能量測，若處理這些項目只能報告靜態正確性及尺寸，不能宣稱執行速度改善。

因使用者跳過實機效能量測，後續只能報告結構、正確性和尺寸結果，不能宣稱啟動加速。

## 2026-10-05：Action 資料範圍與規則收斂

本批依使用者最新優先序，先改善與 MCU 無關的資料模型、可讀性與程式分布；第三方庫及板級硬體優化往後排，保留未來移植 GD32 的彈性。

- `PetBehaviorActionTypes.h` 集中 Action、Outcome、Condition、Effect 型別與容量；`PetBehaviorTypes.h` 保留總配置的組成。
- Action 持有 Outcome／Condition 的連續範圍，Outcome 持有 Effect 範圍。標準、條件與隨機模式共用 Outcome／Effect 陣列，移除子記錄的 `active`、`actionSlot`、`outcomeSlot` 與兩套效果儲存。
- `RuntimeTableBehavior.cpp` 負責 wire 解碼及有界範圍建立；`PetBehaviorRuntimeRules.cpp` 分為 Outcome 選擇、播放選擇、效果計算與原子發布；application 層繼續負責提交 Pet 與要求播放。
- 保留 Runtime Table v9 格式、條件以效果前數值判斷、最低 priority 優先、加權 Outcome、clamp、Daily Change suspension、原子提交與提交後播放失敗不回滾。未改第三方庫、硬體介面、SD 格式或使用者的 `platformio.ini`。

### 尺寸證據

基準原始碼為 `189fb2ec0054a9c9785b6d08be7b3af8e7f0a711`。使用者工作樹已有 `platformio.ini` 修改，board 為 `genericSTM32`；依本次明確選擇，以 `.pio/action-refactor.ini` 將 board 單獨設為 `genericSTM32F103C8`、build_dir 設為 `.pio/action-refactor-build`，其餘 project_29 flags 維持。基準與候選用同一設定與工具鏈重新連結，沒有燒錄。

工具鏈：ST STM32 19.0.0、Arduino STM32 2.9.0、GCC ARM 12.3.1；相依庫為 GFX 1.12.6、ST7735/ST7789 1.11.0、SdFat Adafruit Fork 2.3.103。

| 指標 | 基準 | 本批 | 差額 |
| --- | ---: | ---: | ---: |
| PlatformIO reported Flash | 57,652 B | 57,224 B | -428 B |
| Flash 載入區段合計（含 vector、初始化區段） | 57,948 B | 57,520 B | -428 B |
| 靜態 RAM（.data + .bss） | 7,768 B | 6,792 B | -976 B |

基準 ELF 保存在 `.pio/action-refactor-baseline.elf`；候選在 `.pio/action-refactor-build/project_29/firmware.elf`。以上均為本機忽略產物。Flash 指標需使用同一口徑比較；靜態 RAM 不代表 heap／stack 峰值。

### 驗證

- `test/pet_behavior_runtime/run_host_test.ps1`：修復落後數字動畫契約的舊測試，6／10 Stat 容量均通過；涵蓋三種模式、條件優先序與效果前判斷、隨機權重邊界、選中範圍隔離、非法範圍、重複／無效 Stat 的原子拒絕、clamp 與 suspension。
- `test/runtime_table_behavior/run_host_test.ps1`：通過既有 exporter fixture 與 malformed／legacy 測試；另在現行 exporter fixture 中替換測試用 Action sections，驗證三種模式從 wire 載入後執行及跨 Outcome 借用 Effect 的拒絕。合成測試資料不冒充 Web exporter 的全模式整包輸出。
- `test/game_startup/run_host_test.ps1`、`test/runtime_contract_loader/run_host_test.ps1`：通過。
- `platformio run -c .pio/action-refactor.ini -e project_29`：通過。既有第三方 `boolean` deprecated 與 LTO serial compilation 警告仍存在。
- 未執行 GD32 build、實機 SD／TFT／按鍵、時間量測或 SRAM 峰值量測，不宣稱設備速度提升。

後續優先候選為 Runtime Table reader／各領域 decoder 的責任分布，再處理 Game 狀態收斂。SD 常駐 handle／快取與 lib 精簡仍是獨立批次，不能把本批結果套用為其效能證據。

## 2026-10-05：Runtime Table reader 與外觀解碼責任分布

本批接續 Action 重構後的工作樹，保留先前修改及使用者的 `platformio.ini`。`RuntimeTableBehavior.cpp` 由 1,607 行降為 715 行，這是責任搬移與介面收斂，不代表刪除同等行數的功能。

- `shared/runtime_table/RuntimeTableReader.cpp` 集中 envelope、段落尺寸／範圍、manifest 比對、記錄讀取與數字動畫參照解析；Source callback 可接 SD 或記憶體，沒有 MCU 專用實作。
- `shared/runtime_table/RuntimeTableFile.cpp` 持有 SD snapshot 的開檔、驗證及關檔；Source context 直接指向其持有的檔案，移除只有一個指標的 `FileSource` 包裝。每次查詢仍重新開檔並比對 manifest。
- `appearance/domain/RuntimeTableAppearance.cpp` 擁有 Species、Outfit、Unlock、Evolution 與初始外觀規則及其既有公開查詢。完整載入只使用兩個內部操作：初始外觀解碼與必要段落驗證；初始外觀解碼自行檢查所需的 feature／asset／animation sections。
- `RuntimeTableBehavior.cpp` 保留行為／畫面配置解碼，以及既定的完整載入順序；完整 SD 載入仍直接寫入 caller-owned config，失敗即使本次契約失效，不增加第二份設定。
- `detail/` 標示供解碼器共用的內部介面；未新增 application port、常駐快取、wire 格式、lib 或硬體相依變更。同步修正公開 header 中已過期的 v1／migration 與錯誤發布說明。

### 尺寸與驗證

基準包含上一批 Action 重構。先以同一 `.pio/action-refactor.ini`／`project_29`／STM32F103C8 設定建置並保存 `.pio/runtime-table-split-baseline.elf`，候選仍為 `.pio/action-refactor-build/project_29/firmware.elf`。工具鏈與前一批相同。

| 指標 | 本批前 | 本批後 | 差額 |
| --- | ---: | ---: | ---: |
| PlatformIO reported Flash | 57,224 B | 57,232 B | +8 B |
| Flash 載入區段合計 | 57,520 B | 57,528 B | +8 B |
| 靜態 RAM（.data + .bss） | 6,792 B | 6,792 B | 0 B |

本批主要收益是責任分布與可維護性，沒有 Flash 節省；相較兩批開始前，累計 Flash 減少 420 B、靜態 RAM 減少 976 B。中途單純拆分版本曾為 +20 B，收斂初始外觀前置檢查後最終為 +8 B。

- `test/runtime_table_behavior/run_host_test.ps1` 通過；沿用 exporter／malformed／Action／外觀查詢測試，新增公開載入介面的成功、manifest 不符、截斷、缺檔、null SD、每次重新讀取及關檔生命週期驗證。
- `test/animation_scene_playback/run_host_test.ps1`、`test/custom_layout_export/run_host_test.ps1`（一般與 `-Numeric`）、`test/game_startup/run_host_test.ps1`、`test/runtime_contract_loader/run_host_test.ps1` 均通過。三份直接編譯 RuntimeTableBehavior 的測試腳本已納入拆出的 sources。
- `platformio run -c .pio/action-refactor.ini -e project_29` 通過；最終介面調整後重跑 Runtime Table host 與建置。其他 host 在此次等價前置檢查搬移前已通過。
- 本批檔案的 `git diff --check` 通過；使用者 `platformio.ini` 原有尾端空白不在本批修改範圍。
- 未執行 GD32 build、實機 SD／TFT／按鍵、設備速度或 SRAM 峰值驗證；不宣稱設備效能改善。

## 2026-10-05：Game 啟動準備狀態收斂

本批接續 Runtime Table 拆分後的工作樹。聚焦 Game 的啟動生命週期，保留 MCU 無關的實作、既有 lib、使用者的 `platformio.ini` 與前兩批未提交修改。

- `PreparationState` 取代兩個布林值與一個錯誤字串指標：未準備、已準備、Runtime 失敗、Pet 狀態失敗只能擇一。契約失敗仍禁止重新載入；準備成功後，須經 `finish_setup_game()` 才開放遊戲運作。
- 準備開始即設定預設 Runtime 失敗結果，完成全部流程後才發布 Ready；Pet 狀態失敗另行區分，避免各個錯誤出口重複維護準備結果。
- `GameStartup.cpp` 集中 setup／prepare／finish、初始或儲存狀態恢復及 reset，仍實作原有 Game 方法，不增加物件、配置副本、動態配置或外部介面。`Game.cpp` 從 1,207 行降為 967 行，新檔 244 行；行數只是責任分布指標。
- 保留首錯資源優先序、Debug 階段資訊、相同外觀不重載、不同外觀重載、重置使用啟動選擇及既有存檔順序。`initialized` 與 `runtimeLoadState` 分別表示可運作狀態與契約有效性，沒有硬合併成啟動結果。

### 尺寸與驗證

修改前以 `.pio/action-refactor.ini`／`project_29`／STM32F103C8 成功建置，保存 `.pio/game-startup-baseline.elf`；最終候選為 `.pio/action-refactor-build/project_29/firmware.elf`。工具鏈與前兩批相同。

| 指標 | 本批前 | 本批後 | 差額 |
| --- | ---: | ---: | ---: |
| PlatformIO reported Flash | 57,232 B | 57,272 B | +40 B |
| Flash 載入區段合計 | 57,528 B | 57,568 B | +40 B |
| 靜態 RAM（.data + .bss） | 6,792 B | 6,788 B | -4 B |

本批收益是啟動狀態可讀性與程式碼分布，沒有 Flash 節省。前三批累計相對 57,652／7,768 B 基準，Flash 減少 380 B、靜態 RAM 減少 980 B；不代表 heap／stack 峰值或執行速度。中途版本 Flash 為 57,280 B，集中錯誤狀態設定後為上表最終結果。

- `test/game_startup/run_host_test.ps1` 最終版本通過。新增準備階段禁止 tick／修改、契約失敗後重複 prepare 不再載入、未 prepare 直接 finish、bundle／unlock 啟動失敗，以及 reset 重載失敗後禁止操作的公開介面測試。既有 fresh／restore／fingerprint／reset／Evolution／首錯測試沿用。
- `Game.cpp` 與 `GameStartup.cpp` 在 Debug、Guess Game、Outfit／Appearance Selection、Startup／FirstStart Animation 全開旗標下通過 `g++ -fsyntax-only`。此項只確認條件編譯與型別，不是該組態的連結或執行驗證。
- 獨立 STM32F103C8 PlatformIO 建置通過；既有第三方 boolean deprecated 與 LTO serial compilation 警告保留。
- 任務檔案 `git diff --check` 通過；使用者 ini 原有空白不在本批範圍。Codebase graph 已更新。
- 未執行 GD32 build、實機 SD／TFT／按鍵、時間或 SRAM 峰值量測，未燒錄。

## 2026-10-05：PetActionController 介面精簡與獨立驗收

依使用者要求，由 GPT-6.1 Sol／medium subagent 實作，主代理獨立對照本批修改前檔案、審查行為與執行驗證。Subagent 完成 production 與新增測試程式後因額度耗盡中斷；主代理接手檢查並執行完整的本批驗收。

- `PetActionController` 公開操作從 24 個降至 6 個（均不含建構子）：移除 15 個純 Pet 轉呼叫，以及無現行呼叫者的 `loadOrInitial`、`reset`、`applyEvolutionTarget`。
- `PetBehaviorRuntime` 完全移除對 controller 的相依，直接持有 `Pet &`；CommandExecutor 只持有 `const Pet &`。Game 使用原有 Pet 取得外觀、snapshot 及更新啟動完成旗標。
- Controller 保留 `saveNow`、`maybeSave`、`findEvolutionTarget`、`stageAppearance`、`applyAppearance`、`applyConsumableOutfitUnlock`；保留原有存檔節奏、外觀顯示與儲存順序。Pet 的交易方法、SD 格式、持久化內容、lib 與硬體程式均未改動。
- 修改前檔案保存在忽略目錄 `.pio/next-refactor-before/`，主代理檢查的是本批 delta，不將前三批修改算入本批。未 commit 或 stage。

### 尺寸

同一 `.pio/action-refactor.ini`／`project_29`／STM32F103C8 設定，修改前成功建置並保存 `.pio/subagent-refactor-baseline.elf`；候選為 `.pio/action-refactor-build/project_29/firmware.elf`。工具鏈維持前批版本。

| 指標 | 本批前 | 本批後 | 差額 |
| --- | ---: | ---: | ---: |
| PlatformIO reported Flash | 57,272 B | 57,228 B | -44 B |
| Flash 載入區段合計 | 57,568 B | 57,524 B | -44 B |
| 靜態 RAM（.data + .bss） | 6,788 B | 6,788 B | 0 B |

四批累計相對 57,652／7,768 B 起始基準，Flash 減少 424 B、靜態 RAM 減少 980 B。本批主要收益仍是介面與相依關係精簡；刪除原本已被 linker 排除的函式，不等於同等 Flash 節省。

### 主代理驗收

- 逐檔對照：轉呼叫確實無額外副作用，直接 Pet 操作保留同一物件、參數與呼叫順序；Action 原子提交及 pause 發布仍在動畫解析之前，Status 使用同一份 snapshot 分類與取值。未發現本批需修正的 production 行為差異。
- `test/game_startup/run_host_test.ps1` 通過。新增真實 Pet／PetBehaviorRuntime／AnimationController／CommandExecutor 整合案例：非法重複 Effect 不部分提交也不暫停日變化；缺動畫仍保留多 Stat 提交與兩天 pause；每日天數正常推進；Status 在 Action 前後依最新 Pet 值選不同版本；這些操作不額外存檔。Renderer／SD 為 host fake，並非設備驗證。
- `test/pet_behavior_runtime/run_host_test.ps1` 通過（6 與 10 Stat 容量）；`test/pet_persistence/run_host_test.ps1` 通過。
- 六份受影響 application cpp（Game、GameStartup、PetActionController、PetBehaviorRuntime、CommandExecutor、MinigameController）於 Debug／Guess Game／Outfit／Predict／Appearance Selection／Startup／FirstStart／Sequential Status 旗標全開時通過 `g++ -fsyntax-only`。只證明條件編譯與型別，未將該組態當作完整連結或執行驗證。
- 獨立 STM32F103C8 release 建置通過；既有 boolean deprecated 與 LTO serial 警告保留。任務檔案 `git diff --check` 通過；使用者 `platformio.ini` SHA256 前後一致。
- 未驗證 GD32、實機 SD／TFT／按鍵、設備時間或 SRAM 峰值，未燒錄。
