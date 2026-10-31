# Game 結算與前景狀態重構（2026-10-31）

本輪完成兩階段：先集中 Game 變更後結算及改寫 CommandResult，再統一前景輸入／切換狀態。功能與最大抽籤容量保留。這輪沒有顯著降低記憶體；全功能 Flash 累計僅減少 8 B，其他兩配置略增，靜態 RAM 相同。主要成果是結算順序與前景所有權有單一修改位置。

基準是前一輪 semantic 完成後的來源，HEAD 仍為 `2b47d2c40d1f85547c9ecbc9fdc55e8bc867e407`，加上原有未提交修改。保留其他工作區修改；未 commit、push 或 flash。

## 第一階段：結算政策與 CommandResult

`Game::settleMutation(Settlement)` 集中 UserAction、GuessOutcome、InstructionCompletion、DailyProgress 的死亡判定、解鎖、圖鑑／保存與 base／Evolution 更新。來源使用具名 enum，沒有新增通用事件匯流排、虛擬 pipeline 或一組可任意搭配的 bool。

保留原本不同的順序：

| 來源 | 保存與後續處理 |
| --- | --- |
| UserAction | 已提交效果；解鎖失敗仍繼續原有保存流程；圖鑑採既有延後保存政策，成功後更新 base |
| GuessOutcome | 每次 round／final mutation 各自立即保存，早於下一個結果與動畫；解鎖失敗即停止該次後處理 |
| InstructionCompletion | 先 consume，再一次提交效果；解鎖失敗仍保存已提交結果；保存失敗不能重播算術 |
| DailyProgress | 無圖鑑時保留 maybeSave 的原有位置；解鎖、daily save 成功後才檢查 Evolution，沒有進化才更新 base |

死亡優先於後處理；SD 讀寫錯誤、保存失敗重試與 fatal 資源錯誤仍走既有執行路徑。每日時鐘消耗與 catch-up 排程留在 settlePetDays，不搬入結算函式。

CommandResult 改為一個 `CommandResultKind`（None、UserAction、Playback、Outfit、Collection、Guess、Predict、Instruction），另保留獨立的實際 resourceError。移除 executed、commandId、actionResult 與三個互斥 request bool；Action 的完整執行結果仍在 PetBehaviorRuntime 層，CommandExecutor 只回傳 Game 需要的結算／啟動意圖。

AppliedAnimationMissing 仍是已提交 UserAction＋resourceError；ResourceReadFailed 是 None＋resourceError；Status 播放失敗是 Playback＋resourceError。因此錯誤呈現與「是否已提交效果」沒有混為一談。Guess／Predict 的啟動可用性政策沒有趁此移除。

## 第二階段：前景所有權

AppFlowController 保存唯一的外層前景：Command、Startup、Minigame、Appearance、Collection、Evolution、Battery、FatalError。Game 的輸入與一般畫面分派查這一個狀態；功能自己的 active／phase 只用於內部進度或回報完成。

AppFlowController 集中可觀察 Pet 時間／可在 catch-up 中導航、Evolution 返回目標、Battery overlay 返回目標的政策。刪除舊的 getter／setter 別名與 onMinigameEnded；Game::enterCommand 同時負責 selection dirty。沒有把遊戲內部每一步拉成全域 enum。

Game::cancelForeground 統一清除動畫、Evolution、圖鑑、外觀選擇與 Minigame，用於初始化、成功重設、重新播放 Startup、死亡和 fatal。Instruction-only 編譯也會正確 reset，不再依賴 Guess／Predict 功能旗標。

切換邊界明確化：

- Battery 期間不把按鍵送到背後的選單；一般外觀選單在 Battery 返回後繼續。
- Instruction 被 Battery 中斷時丟棄未結算結果、回到 Command，沿用既有時間排除政策。
- 動畫或立即 Evolution 都會關閉舊 Species 的外觀選單，避免在新 Species 繼續使用舊選項；動畫進化期間由 Evolution 接收前景所有權，Battery 中斷後回 Command。
- 外觀選單自行關閉或圖鑑退出時立即交還前景，保存失敗不會留下已關閉的輸入目標。
- 永久 Alive／Dead 仍由 Pet 的 LifeState 管理；沒有和暫時前景模式合併。

## 相同配置的實際量測

三次皆執行 `test/lottery_capacity/measure_refactor.ps1 -Stage <stage>`，依序為 `game-before`、`settlement`、`foreground-final`。工具鏈、庫、`-Os -flto`、12 Stats、8 Actions、224 Outcomes／2,688 wire effects、各配置容量與功能旗標相同，沿用 [前輪量測方法](memory-refactor.md)。同時進行的容量工作新增 APP_MAX_ACTIONS 可覆寫 macro，但預設仍為 8；這三組沒有啟用其 16-Action／248-Outcome 配置。成功 image 的 Flash bytes 已與 firmware.bin 長度比對。

| 配置 | 重構前 Flash | 結算／CommandResult 後 | 前景最終 Flash | 累計 Flash 變化 | 靜態 RAM（三階段相同） |
| --- | ---: | ---: | ---: | ---: | ---: |
| maximum | 59,688 B | 59,692 B | 59,832 B | +144 B | 9,960 B |
| numeric200 | 62,036 B | 62,104 B | 62,184 B | +148 B | 13,136 B |
| allfeatures | 74,648 B | 74,448 B | 74,640 B | −8 B | 14,964 B |

第一階段全功能省 200 B，但 maximum／numeric 增加 4／68 B；第二階段再增加 140／80／192 B。集中政策不等於自動縮小機器碼，這裡不把類別數、程式行數或 enum 的變化當作容量收益。

maximum／numeric 成功連結。allfeatures 仍是 rejected map，超過 64 KiB **9,104 B**，沒有可燒錄的成功 image。前景開發中間曾產生 74,588／74,624 B 的非最終結果，均不列入上述比較。

RAM 是 `.data + .bss`，不是峰值。ARM ABI 型別探針：CommandResult 7→2→2 B；AppFlowController 2→2→3 B；全功能 Game 三階段均為 13,024 B，增加的前景返回欄位被既有對齊空間吸收。CommandResult 變小不能直接推導 peak stack 節省 5 B，編譯器可能使用暫存器或消除區域物件。

Renderer heap 仍為 4,216 B，其中繪圖／讀取 buffers 為 4,096 B；這些在靜態 RAM 以外。其他 Controller heap、allocator、呼叫堆疊與中斷高水位尚未實測，不能宣稱 allfeatures RAM 已可接受。

## 驗證證據

韌體主驗證使用既有 host runners；Web preflight 通過。沒有移除回歸案例或降低斷言來繞過失敗。

| 驗證 | 結果與範圍 |
| --- | --- |
| 第一階段 Game 一般流程 | 通過；啟動／恢復／重設、每日保存重試、進化、Action／Status、錯誤順序 |
| 第一階段 Instruction＋圖鑑／audio／Death | 通過；延後效果、原子提交、保存失敗不重播、死亡先於進化 |
| 最終 Game 整合 | 通過；FirstStart、外觀、圖鑑、audio、Predict、Guess single／choice、Death enabled |
| Classic Guess／停用 daily post-processing | 通過各自的 Game runner；保留多輪 Guess 結果與 diagnostic 旗標行為 |
| 最終 Instruction | 通過單獨啟用及上述全功能組合；新增中途 reset、保留新生命 Startup、再開新 session |
| 前景輸入整合 | 通過 chooser／Battery／Startup、動畫及立即 Evolution 交接；以實際 preview 查詢、Species、按鍵結果斷言 |
| Death／Action sequence＋RTC | 通過；Death reset rollback、序列替換與中斷、catch-up／導航順序 |
| 最大真實 Lottery 匯出 | 通過 8 Outfit、224 fixed-Version Outcomes、2,688 streamed effects 與全部映射／值 |
| Animation Scene | 通過 atomic first frame、completion、queue、interruption、Idle fallback |
| 真實圖鑑匯出＋生產 SD／像素／儲存 | 通過；公開 API 重新匯出 current runtime，RTC pause remainder、取得條件、重啟及 A/B 中斷保存 |
| 真實 Guess／圖鑑 round＋final 保存 | 通過；兩次立即保存、暫態取得／解鎖、最後保存中斷仍恢復完整 round snapshot |
| 圖鑑啟用 Species／Outfit | 通過；未取得提示、重複使用授予、同目標語意、原子保存、重啟與中斷切換 |
| diff whitespace | 通過 `git -c core.safecrlf=false diff --check` |

兩個既有驗收假設已校正，保留功能驗證：Predict reset 在啟用 FirstStart 時本來就應播放新生命 Startup，現在先驗證並等待它完成才測選單；真實 Guess／圖鑑測試原以為 reset 保留 Guess selection，實際 Game 重設第一個按鈕，現在先導航到 Guess 再測中斷保存。後者在重構前 Game／CommandExecutor／AppFlow 來源同樣失敗；校正導航後前後兩版均通過，沒有修改生產 reset 選擇政策。

Web canonical `scripts/verify.ps1 -TestGroup behavior,evolution -BackendOnly -Completion` 的架構檢查通過，83 tests 中 82 通過、1 error：`test_eight_ordinary_actions_and_eight_lotteries_save_and_export_together` 讀取 ZIP 內不存在的 `profile.ini`。該 Web checkout 有同時進行的 Lottery 變更；本輪未修改 Web 生產／測試來源，也未把此 scoped run 稱為全綠。記錄：`E:/C++/virtualPet/web/.verification/firmware-game-refactor-backend.log`。

## 可重查的本地證據

- `.pio/memory-refactor/{game-before,settlement,foreground-final}/sizes.json`、各 profile map／log、成功 ELF。
- 同目錄 `source/`、`source-hashes.json`、`game-types.cpp/.o/.json`。before 的 Game／Command／AppFlow 來自本輪未修改前的 HEAD；其他依賴使用當時既有 semantic 工作區。settlement 保存當時來源；foreground-final 保存最終來源。
- `.pio/memory-refactor/settlement-*.log` 與 `foreground-*.log`：host 證據。`baseline-guess-collection*.log` 保留原驗收假設的基線重現與校正後通過。

## 速度與未完成項目

沒有修改 AnimationController、SequencePlayback、FrameDecoder、Renderer、codec、TFT 寫入、讀取／行／音訊 buffers、cache 或 12 行 batch，也沒有增加逐幀 metadata／SD 讀取。結算仍在原 mutation 事件邊界，播放 prepare → layout → draw 順序不變。這是來源與 host frame／順序證據，**不是實機 FPS 不退步的量測**。

下一步仍需裝置 FPS／輸入延遲、峰值 heap／stack／中斷量測，以及縮小全功能 Flash 的 9,104 B 缺口。後續容量工作應選有量測收益的來源（例如先前列出的 Timer／UART 與常駐資料配置），不能用縮播放緩衝補這次架構成本。
