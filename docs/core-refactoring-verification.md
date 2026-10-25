# 核心重構驗證紀錄

基準為 `07bc4e4`，使用現行 `platformio.ini` 的 `project_26`、相同工具鏈與容量設定。
前三項重構依序交由 subagent 實作，主代理檢查差異、受影響 host 行為與目標建置後才放行下一項。
第三個 subagent 因工作區額度不足中斷；主代理接手第三項最終驗證與後兩項實作，維持相同的逐項驗收順序。
未提交、推送或燒錄；實機 SD／TFT、低電壓、RTC／STOP 與記憶體峰值仍需設備驗收。

## 建置紀錄

Flash 為 `text + data`，靜態 RAM 為 `data + bss`，不包含 heap／stack 峰值。

| 階段 | Flash bytes | 靜態 RAM bytes | 日誌 |
| --- | ---: | ---: | --- |
| 重構前 | 61448 | 11632 | `.pio/sequential-baseline-build.log` |
| 1 Runtime Table 拆分 | 61448 | 11632 | `.pio/sequential-phase1-build.log` |
| 2 電壓判斷集中 | 61380 | 11624 | `.pio/sequential-phase2-build.log` |
| 3 動畫影格準備狀態 | 61352 | 11624 | `.pio/sequential-phase3-build.log` |
| 4 日結算回傳時鐘消耗量 | 61368 | 11624 | `.pio/sequential-phase4-build.log` |
| 5 存檔結果明確化 | 61380 | 11624 | `.pio/sequential-phase5-final-build.log` |

重跑目前 profile 的命令為 `pio run -e project_26`。

## 已確認的基準

- `test/game_startup/run_host_test.ps1` 一般組態通過。
- 同一入口的 FirstStart、Appearance、Collection、Death 同開組態通過。
- 同一入口的 `-EnableSequenceAudio -IdleAudioOnly` 通過。
- `test/runtime_table_behavior/run_host_test.ps1` 在修改前即於 `testDeathCapability` 失敗。Web 共用 fixture 已為 v26，主線 reader 接受 v22；這項基準失敗不能算成重構通過證據。

Host 輸出遇到既有 `.pio` executable 寫入權限時，改用新的忽略路徑。
更換輸出路徑不變更測試條件。另新增電壓、影格準備及存檔結果斷言，既有存檔斷言改為比較明確結果。
正式 profile 保持原樣。

## 1 Runtime Table 責任拆分

載入協調留在 `RuntimeTableBehavior.cpp`；行為解碼與畫面解碼各有獨立實作，僅由 detail header 連接。
逐段比對確認函式內容未變，保留現有檢查與執行順序、單次開檔和 caller-owned config。

契約載入、版面像素與外觀重繪 host 通過。解碼 Death 0／1 使用歷史 Web `416a9de7` 的原始 v22 coherent fixture，
其餘無歷史版本的 generated fixtures 使用忽略目錄內的 v22 header 測試副本。
同一組補充資料在原 HEAD 與重構版本均通過；副本僅支持本次行為比較，不代表 v26 契約相容或 Web 完整驗收。
主代理核對差異並重跑解碼組後，放行第二項。

音訊開啟的對照組僅將暫存 `.pio/sequential-audio.ini` 的 `ENABLE_SEQUENCE_AUDIO` 改為 1。
此時 Flash 65228、靜態 RAM 15340 bytes，日誌 `.pio/sequential-audio-before-build.log`。

## 2 電壓判斷集中

`VoltageMonitor` 集中電壓狀態、採樣與恢復計時、低電壓 STOP 判斷及喚醒狀態。
`main.cpp` 保留 PVD、按鍵、音訊與螢幕效果，以及 STOP 前後的硬體順序。
原先重複的 low-battery boolean 由 Critical 狀態取代。

`rtc_power` 通過 100 ms 採樣、5000 ms STOP／逐級恢復、震盪、暗畫面喚醒、開機與 uint32 rollover 邊界。
主代理另重跑 600000 次決策比較，結果與原 HEAD 的 awake 判斷及原 wake 分支相同，並核對硬體動作的順序後放行。
音訊對照建置通過，Flash 65164、靜態 RAM 15340 bytes，日誌 `.pio/sequential-phase2-audio-build.log`。

## 3 動畫影格準備狀態

`PreparedFrame` 以 Unprepared／Hold／FirstFrame／Advance 取代三個布林旗標。
取消、取代序列、換 base、重繪與 tick 完成都使舊準備狀態失效。
音訊路徑保留先完成首影格傳輸再啟動 sample clock 的順序。

主代理通過新影格生命週期與外觀重繪測試，並使用歷史 v22 原始 fixture 跑完整 atomic scene 測試。
Game 的一般、FirstStart／Appearance／Collection／Death 組合、音訊 idle 與序列中斷 host 通過。
檢查每個狀態寫入及目標建置後放行第四項。
音訊對照為 Flash 65128、靜態 RAM 15340 bytes，日誌 `.pio/sequential-phase3-audio-build.log`。

## 4 日結算回傳時鐘消耗量

`settlePetDays(dueDays)` 直接回傳時鐘應消耗的天數，取代 `DayResult` 與可變 `days` 輸出參數。
受阻或 fatal 回傳 0；可恢復錯誤仍按既有規則消耗本次天數，死亡則由原生命週期提早結束。
保留首日事件結算、後續穩定區間跳過，以及 unlock／collection／evolution／save 的順序。

Game 一般、FirstStart／Appearance／Collection／Death、daily post 關閉、Death 關閉、音訊 idle、
音訊加 Collection 的序列中斷與 RTC 補時 host 均通過。
主代理比對原方法所有 return 分支及時鐘消耗後放行第五項。
相較第三項 Flash 增加 16 bytes；音訊對照增加 4 bytes 至 65132，靜態 RAM 15340 bytes，容量仍符合原板設定。

## 5 存檔結果明確化

`PetSaveController` 的請求回傳 NotNeeded／Deferred／Persisted／Failed，只有 Persisted 表示實際寫入成功。
內部 `saveNow` 改名 `requestSave`，所有呼叫者同步遷移；`maybeSave` 與 `endBatch` 也回傳結果。
`Game` 的既有 boolean 介面仍表示請求未失敗，保留接受批次請求的語意。
週期寫入失敗仍走既有重試節奏，未新增錯誤畫面或自動重試規則。

新增測試確認不需寫入、批次延後、合併後寫入最終狀態、flush 失敗與後續請求重試、
生命狀態與 immediate collection 事件繞過批次，以及重複結束不重寫。
Game 一般、FirstStart／Appearance／Collection／Death、daily post 關閉、音訊 idle、
Guess／Predict／Collection／音訊組合與 Death 0／1 host 通過，Pet persistence 的 Death 0／1 也通過。
呼叫端保留既有失敗判定，死亡存檔明確要求 Persisted；未保留舊 controller boolean 相容介面。

最終正式組態 Flash 61380、靜態 RAM 11624 bytes，比基準少 68 與 8 bytes。
音訊對照 Flash 65144、靜態 RAM 15340 bytes，日誌 `.pio/sequential-phase5-audio-final-build.log`。
這些數字只代表相同工具鏈和固定 flags 的 linked 容量，不代表執行速度或裝置記憶體峰值。

## 後續 v26 升級

2026-10-25，後續升級已使韌體直接讀取共用 v26 fixture，先前記錄的 v26／v22
版本不相容已解除；上述基準與重構階段證據仍保留原意。升級後的測試、功能整合及容量見
[Runtime Table v26 升級驗證](runtime-v26-upgrade-verification.md)。
