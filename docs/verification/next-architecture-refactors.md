# 後續架構重構與容量比較（2026-10-31）

## 最終採用的變更

1. **數值 context 建立集中**：`makeRuntimeValueContext` 統一欄位初始化；Pet State、Status、Action 條件、外觀查詢及數值版面改用同一入口。保留原本各用途的 mask 計算，以及是否提供 Species／Outfit 的差異；沒有新增 service、常駐 context 或 SD 讀取。
2. **runtime loader 初始化與錯誤整理**：完整 table loader 繼續擁有成功載入路徑的初始化。外層只在尚未進入該 loader 就失敗時清空輸出；bundle／table 錯誤共用一次名稱整理。保留公開的 initial／active appearance 入口、完整 loader 的獨立使用契約及初始外觀已解析但後續解碼失敗的旗標語意。沒有移除實際讀取、bundle 身分或有界執行保護。
3. **Instruction 呈現責任拆分**：具體 `InstructionPresentation` 擁有一場 Instruction 的規則物件、素材／版本選擇、影格排程與 visual-ready 協調。`InstructionResponse` 保留純規則與輸入時間判定；`MinigameController` 保留遊戲選擇、輸入分派、取消及完成結果轉交。移除原 controller 的呈現欄位與 `renderInstruction`，沒有相容轉接層或遊戲繼承階層。
4. **播放互斥狀態收斂**：Base／Sequence 的同步模式由一個 enum 表示，互斥模式共用起播旗標；刪除只有寫入而沒有讀取的 `synchronizedRepeat`。不合併時間演算法、queue／duration 快取或工作緩衝，不加入逐幀 virtual dispatch。原本 prepare → layout → draw、首幀完成後才啟動音訊、loop／repeat／following 行為保留。

Game 仍為協調者。它的終身 controller 改為直接成員的方案已實作、測試及量測，但因全功能 Flash 回退而撤回，詳見下表。

## 逐階段實際量測

相同 genericSTM32F103C8、`-Os -flto`、函式庫與三組功能旗標。前三配置固定 8 Actions／224 Outcomes，numeric 為 200 rules＋Guess；全功能配置保留全部選配功能。Flash 取 linker sections；成功配置皆比對 `firmware.bin` 長度。固定 owned heap 使用相同 profile defines 在 Cortex-M3 ABI 下編譯實際型別，以 nm 讀取大小。

| 累積階段 | maximum Flash | numeric Flash | allfeatures Flash | allfeatures static RAM | allfeatures 固定 owned heap | allfeatures 已知常駐小計 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 本輪基線（Renderer 拆分後） | 59,824 | 62,172 | 74,528 | 16,092 | 3,912 | 20,004 |
| context | 59,752 | 62,100 | 74,456 | 16,092 | 3,912 | 20,004 |
| loader | 59,792 | 62,088 | 74,420 | 16,092 | 3,912 | 20,004 |
| InstructionPresentation | 59,792 | 62,088 | 74,404 | 16,092 | 3,912 | 20,004 |
| 最終：播放狀態 | **59,792** | **62,088** | **74,396** | **16,092** | **3,908** | **20,000** |

全部為 bytes。loader 對 maximum 單獨增加 40 B，對另外兩配置分別減少 12／36 B；因此不以刪除程式行數推估收益。最終相對基線節省 Flash 32／84／132 B。

maximum 的 static RAM／固定 heap／小計各階段保持 11,096／3,540／14,636 B；numeric 保持 14,264／3,588／17,852 B。全功能 AnimationController 由 372→368 B，因此只有開啟 sequence audio 的全功能配置少 4 B 固定 heap。

獨立額度另以相同基本功能配置、16 Actions／248 Outcomes 實測：Flash **59,836→59,800 B**，static RAM 11,720 B、固定 heap 3,540 B、已知常駐小計 15,260 B 均不變。真實 Web 匯出的 8 普通＋8 Lottery、248 Outcomes 與 button slot15 另由 host 執行確認；沒有把這組基本配置誤稱為全功能＋248 Outcomes 的連結證據。

read buffer 1,024 B、line buffer 3,072 B、audio buffer、12 行 batch、metadata／播放快取保持原尺寸。Renderer heap 3,096 B、line offset 24 B、8 B alignment 保持不變。

已知常駐小計為 static RAM 加上專案自行持有的固定 heap 物件大小，**未含 allocator 管理與向上對齊、其他函式庫 heap、stack 或 ISR**。全功能仍超出 64 KiB Flash **8,860 B**，沒有成功可燒錄 image；20,000 B 小計也不是裝置峰值 RAM 驗收。

## 已撤回：Game 直接持有終身 controller

此實驗以 InstructionPresentation 階段為比較基線，將 Game 的 unique_ptr controller 改為具體成員，保留建構／解構順序及功能旗標。一般與 Instruction 多功能、真實 Guess／activation 回歸通過。

| 配置 | Flash 前→實驗 | static RAM 前→實驗 | 固定 heap 前→實驗 | 已知常駐小計前→實驗 |
| --- | ---: | ---: | ---: | ---: |
| maximum | 59,792→59,708 | 11,096→11,520 | 3,540→3,096 | 14,636→14,616 |
| numeric | 62,088→62,088 | 14,264→14,728 | 3,588→3,096 | 17,852→17,824 |
| allfeatures | 74,404→74,864 | 16,092→16,876 | 3,912→3,096 | 20,004→19,972 |

全功能少 32 B 已知常駐，卻增加 **460 B Flash**。在目前 Flash 缺口與使用者優先目標下不採用；最終三個 Game 檔案與拆分後、實驗前來源逐檔比對相同。不能以 `.bss` 上升或 heap 消失單獨判定 RAM 收益。

## 功能與 I/O 驗證

- 最終 Game 多功能及 Instruction-only：重開／保存、FirstStart、Guess／Predict 切換、外觀／圖鑑、Death；Instruction 慢速畫面、feedback 最後一幀 hold、取消／讀取失敗丟棄效果、延後一次結算與保存失敗不重播通過。
- Game idle audio：RTC／日界補時、Action、STOP、死亡及外觀選擇中斷通過。
- 真實圖鑑 Guess：round／final 保存、解鎖／acquisition、reboot 與中斷保存恢復通過。真實 Species／Outfit activation、同目標語意及原子保存通過。
- Animation Scene 的原子首幀、completion、queue、interruption／Idle fallback 通過；音訊實際匯出 per-repeat／following、cutoff、短 EOF、效果一次、bounded retry 及取消通過。
- PetBehaviorRuntime 6／10／12 Stats、效果／pause 與 catalog gating、Status、runtime table 及 loader 錯誤路由通過。loader mock 補齊完整 loader 原有初始化契約，保留原斷言並增加 early-failure 輸出狀態斷言。
- Instruction 純規則與按鍵 debounce／deadline／rollover 矩陣通過；新呈現 module、MinigameController 及兩個播放來源在 DEBUG＋多功能 defines 下 syntax-only 通過，這不等於 debug target 連結驗收。
- 最大 Lottery 真實匯出：8 Outfit／224 fixed-Version Outcomes／2,688 effects 全映射通過；獨立 248 Outcomes／button slot15 通過。
- Web canonical `behavior,evolution,screen-layout,export -BackendOnly -Completion`：**156 tests＋backend architecture 通過**。不是全 Web suite。

對本輪前快照與最終來源執行相同真實 pixel/layout workflows，每個案例的 File API 計數逐行比較一致：

| 工作流程 | 案例數 | open | read | read bytes |
| --- | ---: | ---: | ---: | ---: |
| custom layout | 7 | 1,070 | 17,988 | 4,225,074 |
| numeric200 | 1 | 1,110 | 9,526 | 3,045,217 |

原有像素斷言亦通過。這是受測流程的 API 次數／資料量一致，**不是 I/O 呼叫排序、裝置 FPS、SPI throughput 或輸入延遲證明**。播放速度仍需裝置基線與實測，不以 host 測試取代。

## 額外檢查發現的 Web 契約不一致

額外 canonical `instruction-response -BackendOnly -Completion` 執行 11 tests，有 **2 項 failure**：`test_public_save_rejects_invalid_ranges_weights_and_distribution_shapes` 與 `test_type_distribution_rejects_bad_shapes_and_old_configuration_gets_default`。API 接受 `mode=equal` 但附帶 weights 的輸入並回傳 200，測試預期 400。這些測試不執行本輪 C++；本輪沒有修改 Web 生產／測試來源，也沒有為了通過而更改其權重語意。此分區不能宣稱驗證通過。

## 證據與重跑

- 量測：`test/lottery_capacity/measure_refactor.ps1 -Stage <name>`、`probe_owned_heap.ps1 -MeasurementRoot <dir>`。量測工具現使用各 stage 的 config／build 目錄；owned-heap 探針依實際 Game 成員所有權判斷固定 heap，避免 inline 成員已在 static RAM 中卻重複相加。
- 階段資料：`.pio/memory-refactor/next-before/`、`next-context/`、`next-loader/`、`next-instruction-final/`、`next-owned/`（撤回）、`next-final/`；獨立額度為 `next-before-independent/`、`next-final-independent/`。
- 來源快照、SHA-256、相對於本輪前的 diff、host logs 與逐案例 I/O 比較：`.pio/next-refactors/`。快照中的 baseline host runner 只調整 fixtures 絕對路徑；production 不變。
- 系統 `python` 別名造成最初搬移腳本未執行；該輪結果及之後共用 build 的碰撞試跑排除。實際搬移改用 Web 專案 venv Python，重跑 Instruction-only／multi 與 `next-instruction-final`；後續 build 隔離且成功配置比對 binary 長度。
- Web logs：`E:/C++/virtualPet/web/.verification/next-refactors-backend.log`、`next-refactors-instruction-backend.log`。

保留先前及其他工作區變更。未 commit、push 或 flash；實機速度／峰值 RAM 與剩餘 Flash 缺口仍待處理。
