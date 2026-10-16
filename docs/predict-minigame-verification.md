# 占卜小遊戲與音訊介面清理驗證（2026-10-16）

## 實作範圍

`PredictGame` 接管原本 `CommandExecutor` 的占卜序列，透過既有
`MinigameController` 管理開始、更新、完成、失敗與中斷。沿用 Predict 指令、
`ENABLE_COMMAND_PREDICT` 與既有 SD 動畫角色，不新增 Web 選項或修改匯出格式。
占卜不接收猜物的左右選擇；完成後返回指令選單。RTC 保留原本排除占卜播放時間的行為。

音訊只保留現行動畫歸屬路徑；移除 action/idle 音訊資料欄位、舊載入器、
`AnimationSequence.audio`、`setBaseAnimation` 舊參數及多餘的 `baseAudio()` 查詢。
現行 ANIMATION_AUDIO 的讀取界限、實際引用檢查與共用 envelope 檢查保留。
跨層產品規格仍以 Web `.scratch/sequence-audio/spec.md` 與
`contracts/runtime_table_v21.json` 為準。

## Host 驗證

以下命令皆已通過，從韌體 repository 根目錄執行：

```powershell
# 原有完整回歸；占卜與猜物關閉。
./test/game_startup/run_host_test.ps1

# 原有開機、外觀與收藏路徑。
./test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation -EnableAppearanceSelection -EnableCollectionCatalog

# 猜物單獨編入，以及占卜單獨編入（音訊開／關）。
./test/game_startup/run_host_test.ps1 -EnableGuessGame
./test/game_startup/run_host_test.ps1 -EnablePredict -PredictOnly
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableSequenceAudio -PredictOnly -DeathEnabled 1

# 兩個小遊戲共同編入：三回合原模式與正式 profile 的單回合選邊模式。
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableGuessGame
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableGuessGame -EnableSequenceAudio -PredictOnly -DeathEnabled 1
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableGuessGame -EnableGuessSingleRound -EnableGuessPlayerChoiceResult -EnableSequenceAudio -DeathEnabled 1

# Base 配樂不阻塞 RTC，且在動作、睡眠、死亡與外觀選單中斷。
./test/game_startup/run_host_test.ps1 -EnableSequenceAudio -IdleAudioOnly -DeathEnabled 1 -EnableAppearanceSelection

# 真實 Web v21 匯出：動畫配樂、靜音與未編入音訊的拒絕路徑。
$fixtures = 'E:/C++/virtualPet/web/tests/fixtures/sequence_audio'
./test/sequence_audio/run_host_test.ps1 -FixtureRoot "$fixtures/animation_music" -Animation
./test/sequence_audio/run_host_test.ps1 -FixtureRoot "$fixtures/no_audio" -Silent
./test/sequence_audio/run_host_test.ps1 -FixtureRoot $fixtures -Disabled
```

新增覆蓋包含 11 個固定種子的結果角色、缺少資源、輸入路由、音訊播放時長、
RTC 暫停、播放失敗、重開、低電量與睡眠中斷、死亡後重開，以及猜物結算後再開始占卜。
音訊失敗覆蓋已搬至 v21 動畫配樂路徑，保留 SD 讀取失敗、硬體啟動失敗、
underrun、效果不回滾、循環與取消檢查。舊 v20 整段／狀態歸屬播放斷言不再執行。

完整回歸另修正兩個既有過時預期：重開失敗會嘗試回滾外觀，因此載入共兩次；
日界交易在進化查詢失敗前已提交，因此 Fatal 阻止的是後續日界。
未修改的 HEAD 快照可重現第一個失敗。猜物測試的顯示替身也改為保持循環提示，
避免把第一幀誤判為整段完成。以上僅修正測試，未調整對應韌體行為。

## STM32F103C8 建置

以 `genericSTM32F103C8`、`-Os`、LTO、Debug=0、12 Pet Stats、1 Screen Rule、
開機及 FirstStart 開啟建置。猜物開啟時使用單回合、玩家選邊結果。
`platformio.ini` 保留原樣；附加測試設定與輸出在忽略的 `.pio/` 目錄。

| 組合 | Flash / 65,536 bytes | 靜態 RAM / 20,480 bytes | 結果 |
|---|---:|---:|---|
| 既有 project_26 | 60,648 | 7,152 | 通過 |
| 占卜 | 61,164 | 7,152 | 通過 |
| 占卜＋猜物 | 62,812 | 7,440 | 通過 |
| 音訊＋占卜 | 65,036 | 10,868 | 通過；Flash 剩 500 bytes |
| 音訊＋占卜＋猜物 | 連結時超出 1,560 bytes | 未取得成功連結結果 | 不符合 C8 容量 |

附加測試設定沿用 `env:profile_base` 的共用編譯選項，明確指定上述功能；
不能用重複 `-D` 加 `-U` 覆蓋功能，PlatformIO 的旗標整理可能使實際功能回到預設關閉。
可用下列命令重現本次附加建置（設定檔是本次本機驗證產物）：

```powershell
$env:PLATFORMIO_BUILD_DIR = "$PWD/.pio/ponytail-build-20261016"
pio run -e project_26
pio run -c .pio/ponytail-profiles-20261016.ini -e ponytail_predict -e ponytail_predict_guess -e ponytail_audio_predict -e ponytail_audio_predict_guess
```

容量是本次連結結果，未宣稱相對前版的大小或速度收益。靜態 RAM 不含所有執行中
heap/stack 用量。未提交、推送或燒錄，未完成裝置音質、同步、電流與互動驗收。

## 後續簡化 1–3 驗證（2026-10-16）

僅移除 presentation 解碼已判空後的重複條件、以函式內 helper 共用三段開機動畫
時長計算，以及將 `renderPrepared()` 的播放失敗角色／旗標紀錄收斂至一處。
保留既有讀取與驗證、開機順序、FirstStart 存檔時機、固定幀與一般動畫的失敗判斷。
本輪未修改測試或測試預期。

以下 host 回歸通過：

```powershell
./test/runtime_table_behavior/run_host_test.ps1
./test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation -EnableAppearanceSelection -EnableCollectionCatalog
./test/animation_scene_playback/run_host_test.ps1
./test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation -EnableAppearanceSelection -EnableCollectionCatalog -EnableSequenceAudio
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableGuessGame -EnableGuessSingleRound -EnableGuessPlayerChoiceResult -EnableSequenceAudio -DeathEnabled 1
./test/sequence_audio/run_host_test.ps1 -FixtureRoot E:/C++/virtualPet/web/tests/fixtures/sequence_audio/animation_music -Animation
```

Animation Scene 原輸出 exe 因 Windows 權限無法覆寫；實際執行同一份 harness，
只在記憶體中改用 `.pio/simplification-animation-scene-20261016.exe` 輸出路徑後通過，
未變更 harness 檔案。

以相同 C8 profile、工具鏈與選項比較本輪簡化前後：

| 組合 | 簡化前 Flash | 簡化後 Flash | 靜態 RAM | 簡化後 Flash 餘裕 |
|---|---:|---:|---:|---:|
| 現行 project_26 | 60,648 | 60,580 | 7,152 | 4,956 |
| 現行 project_26 僅開啟音訊 | 64,584 | 64,516 | 10,860 | 1,020 |

兩組均成功連結，Flash 各少 68 bytes，靜態 RAM 不變；此數字只代表這兩組設定。
音訊設定為 `.pio/current-profile-audio-20261016.ini`，與原 `platformio.ini` 僅差
`ENABLE_SEQUENCE_AUDIO=1`；正式 profile 未修改。建置紀錄位於
`.pio/simplification-1-3-noaudio-20261016.log` 與
`.pio/simplification-1-3-audio-20261016.log`。建置警告包含 Adafruit 的 `boolean`
棄用提示及 LTO 序列編譯提示。
本輪未燒錄或驗證實機效能。

## 後續簡化 4–5 驗證（2026-10-16）

`MinigameController::showsResourceErrorOnPlaybackFailure()` 獨立表達失敗顯示策略，
`pausesPetTime()` 僅供時間暫停判斷。兩者目前都對 Predict 回傳 true，保留占卜顯示
資源錯誤、猜物交由自身流程處理的差異；顯示仍先於小遊戲的失敗清理。

`Game::handleCommandResult()` 保留死亡、是否執行與資源錯誤判定，再分派至
`handleAppliedUserAction()` 與 `handleSystemCommandRequest()`。動作後的解鎖、
收藏保存與基底動畫更新順序，以及收藏、換裝、占卜／猜物的請求優先序均不變。
未新增控制器、狀態欄位、匯出資料驗證或修改測試預期。

依使用者要求只執行相關驗證，未跑整個 repository 測試。下列分組回歸通過：

```powershell
# 第 4 項完成後：占卜失敗顯示、RTC 暫停與猜物切換。
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableGuessGame -EnableGuessSingleRound -EnableGuessPlayerChoiceResult -EnableSequenceAudio -PredictOnly -DeathEnabled 1

# 第 5 項完成後：動作結果、開機、外觀／收藏，以及小遊戲分組。
./test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation -EnableAppearanceSelection -EnableCollectionCatalog -EnableSequenceAudio
./test/game_startup/run_host_test.ps1 -EnablePredict -EnableGuessGame -EnableGuessSingleRound -EnableGuessPlayerChoiceResult -EnableSequenceAudio -DeathEnabled 1
```

額外將上述 FirstStart、外觀、收藏、音訊、占卜、猜物與死亡功能同時開啟時，
`testPredictMinigame()` 的 reset 分支在 `assert(!game.hasTransientAnimation())`
失敗：reset 會排入 FirstStart，而該測試預期沒有動畫。
使用本輪修改前的 Game.cpp 與 MinigameController.cpp 快照執行相同 harness，
可重現同一行失敗。此組合未通過，未為本次重構修改程式行為或放寬斷言。

STM32F103C8 三組建置均成功；正式 `platformio.ini` 未修改：

| 組合 | Flash / 65,536 bytes | 靜態 RAM / 20,480 bytes | Flash 餘裕 |
|---|---:|---:|---:|
| 現行 project_26 | 60,580 | 7,152 | 4,956 |
| 現行 project_26 僅開啟音訊 | 64,516 | 10,860 | 1,020 |
| 音訊＋占卜（ponytail_audio_predict） | 64,972 | 10,868 | 564 |

前兩組與簡化 1–3 後的容量相同；第三組為本輪實測值，未做本輪前後容量比較。
建置紀錄位於 `.pio/simplification-4-5-noaudio-20261016.log`、
`.pio/simplification-4-5-audio-20261016.log` 與
`.pio/simplification-4-5-audio-predict-20261016.log`。
警告仍包含 Adafruit `boolean` 棄用與 LTO 序列編譯提示。
`git diff --check` 通過；未提交、推送或燒錄，未做實機驗收。
