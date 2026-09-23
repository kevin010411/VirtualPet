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

## 重構前：啟動讀取現況

`Game::prepare_game` 依序呼叫 `loadRuntimeManifest`、`SdAppearanceLoader::findInitialAppearance`、`Game::configureActiveAppearance`。最後一項透過 `loadRuntimeContract` 再讀 manifest，接著 `loadCompleteRuntimeTable` 讀完整表，並以 `SdAppearanceLoader::validateRuntimeContracts` 再讀外觀。這些步驟各自開啟 `/runtime.bin`，所以在這段路徑內至少開檔五次。程式位置：`src/presentation/application/Game.cpp`、`src/pet_behavior/domain/RuntimeContractLoader.cpp`、`src/pet_behavior/domain/RuntimeTableBehavior.cpp`、`src/appearance/adapters/SdAppearanceLoader.cpp`。

完整表讀取會檢查 envelope 與已取得 manifest 的檔案大小、bundle ID、schema fingerprint、file CRC，再解碼行為、顯示與外觀。載入失敗可能回報 `runtime.bin` 或 BundleReader 記錄的第一個資源；外觀驗證另有第一個資源錯誤。`PetBehaviorConfig` 約 6 KiB，目前直接清空並解碼到呼叫者持有的緩衝區，以免建立第二份大型 stack candidate。後續改動必須保留這些驗證、錯誤順序、低 SRAM 用量、失敗後進入 fatal/reboot 的語意。

首次啟動的儲存狀態可能恢復不同 species/outfit，此時 `prepare_game` 會再次呼叫 `configureActiveAppearance`。日後切換外觀也共用這條路徑。因此不可把啟動時取得的 manifest 無條件當成整個工作階段永久有效，也不可把首次與後續外觀載入的驗證需求混為一談。

## 已實作：共用 manifest 與完整表讀取

`prepare_game` 先讀取 manifest，再呼叫 `loadInitialRuntimeContract`。後者以已讀取的 manifest 開啟完整表一次，先解析初始 species/outfit，再解碼該外觀的行為與顯示設定；同一讀取仍執行 envelope 比對、bundle 引用、idle animation 及必要外觀段落檢查。原本 `SdAppearanceLoader::validateRuntimeContracts` 的檢查只驗證 Appearance、Species、Outfits、OutfitUnlocks，以及啟用演化時的 Evolutions 段落是否存在，已移到完整表讀取後段；不再為此重開 `/runtime.bin`。設定直接寫入 `Game` 持有的 `PetBehaviorConfig`，沒有第二份約 6 KiB 暫存。初始外觀解析失敗與其後的設定失敗仍可區分；恢復不同外觀及一般外觀切換仍重新讀取 manifest 與完整表，且執行相同段落檢查。

依原始碼路徑，初始外觀設定完成前的 `/runtime.bin` 開檔由重構前至少五次降為兩次：manifest、初始外觀與完整設定合併讀取。首次啟動後的 `enterSpecies` 目前仍重讀 manifest 與完整表；解鎖與存檔恢復也可能繼續開檔，所以此計數不代表整個啟動流程總開檔數，更非時間量測。

2026-09-24 驗證：`pio run -e project_12` 成功，Flash 61,628 B（較基準 +132 B，較上一步 -124 B）、靜態 RAM 6,840 B（持平）、BIN 61,924 B（較基準 +132 B）；`test/runtime_table_behavior/run_host_test.ps1`、`test/runtime_contract_loader/run_host_test.ps1` 通過；`git diff --check` 通過。Runtime Table host 測試使用完整外觀 fixture，驗證同一次開檔回傳初始外觀與對應設定、後續外觀同樣單次開檔、缺少必要外觀段落會拒絕，以及 manifest 不符時停止解析。載入器 host 測試以測試替身驗證錯誤階段、資源傳遞及後續外觀重新載入。未直接執行 `Game::prepare_game` 或實機 SD 讀取，實機執行依使用者指示跳過。

## Flash Optimization：已實作與量測

`project_12` 的連結 ELF 顯示，Arduino `random/randomSeed` 會帶入 newlib `rand/srand`；後者的配置失敗斷言再帶入 `__assert_func`、`fprintf` 與 stdio。實際選隨機 Status、Action 結果、動畫版本與 Guess Game 只需要有界整數，現在共用 `FirmwareRandom` 的一個 32 位元狀態與 xorshift32。啟動仍使用原本的 `analogRead(0)` 作為種子；零種子保持現有狀態，零上界回傳零；範圍與單次抽樣次數維持不變，但相同種子下的抽樣序列不同於 newlib `rand`。所有既有第三方庫與原本使用的圖形、SD 功能保持在建置中。

同一工作樹、同一 `project_12` 設定的 A/B 連結比較：更動前 Flash 61,628 B、靜態 RAM 6,840 B、BIN 61,924 B；更動後 Flash 57,992 B、靜態 RAM 6,828 B、BIN 58,288 B。Flash 與 BIN 各減少 3,636 B；相對乾淨 HEAD 基準 Flash 減少 3,504 B。更動後 `nm` 不再出現 `rand/srand`、`__assert_func`、`fprintf`、`_vfiprintf_r`。`test/firmware_random/run_host_test.ps1` 驗證範圍、固定種子重現、零種子與零上界；`test/animation_scene_playback/run_host_test.ps1`、Runtime Table 與 Runtime Contract Loader host 測試通過，PlatformIO 建置成功。未在實機驗證亂數分布、遊戲互動或效能。

## 後續擬議實作順序

1. **繼續 Flash 熱點拆解。** 以 `project_12` 的 LTO ELF/map 找仍保留的應用程式與執行庫符號；優先找跨模組重複的解析、錯誤格式化與抽象層成本。每次只做一個可回退的改動，用同一環境 A/B 建置並核對功能與 RAM，不能只依原始碼行數預估。不可藉移除目前使用中的第三方庫節省 Flash。
2. **補齊 Game 啟動整合驗證。** 現有 host 測試未直接覆蓋 `Game::prepare_game` 的 renderer/fatal 狀態與存檔恢復分支；後續若有可維護的測試替身，再加入缺檔、損毀、不同外觀恢復和第一錯誤資源案例。
3. **評估首次啟動重複載入。** `enterSpecies` 目前即使 species/outfit 與已載入的初始外觀一致，也會重讀 manifest 與完整表；須先確認初始狀態建立、renderer 設定與錯誤處理的順序，再決定是否安全略過這次重讀。
4. **其他效能候選。** Renderer 行緩衝、每幀 pack 存取與一次性配置均未實作；使用者已跳過實機效能量測，若處理這些項目只能報告靜態正確性及尺寸，不能宣稱執行速度改善。

因使用者跳過實機效能量測，後續只能報告結構、正確性和尺寸結果，不能宣稱啟動加速。
