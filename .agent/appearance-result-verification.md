# 外觀／Evolution 結果介面（2026-10-06）

- Evolution check/update 使用 NoChange、InProgress、Completed、Failed、FatalFailure。只有 NoChange 允許 Game 的當次 Pet tick 繼續；其餘結果停止本次每日推進／週期存檔協調。
- EvolutionHost 保留 AppearanceChangeResult，ConfigurationFailed 轉為 FatalFailure，其餘物種套用失敗為 Failed。Evolution 不再直接顯示錯誤；Game 接收結果後處理提示與 fatal。
- 外觀配置／啟用回呼失敗不再寫 runtimeLoadState 或 flow；成功仍發佈已啟用契約。Game 在 Evolution、外觀選擇、Session prepare/reset 的結果接收位置處理全域狀態。
- Session reset 回傳 AppearanceChangeResult，保留契約、Pet 狀態、解鎖、存檔失敗身份。已發生變更不回復。目標外觀已提交後的非資源逐幀播放失敗保留原本靜默回到 base 的行為，Evolution 生命週期回報 Completed。
- Game 的 enterRuntimeFatal 統一設 runtime Failed、取消播放與進化、進入 fatal 並顯示資源錯誤。啟動 prepare 保留平台顯示初始化完成前不顯示的順序。

驗證：

- game_startup host 修改前後通過；新增直接 Evolution 結果測試：無目標／同物種、查詢失敗、五種物種套用結果、缺來源動畫、進行中、來源播放失敗、來源段完成後契約失败。確認 controller 不自行顯示錯誤。直接 Session reset 測試確認 ConfigurationFailed 傳出、不自行顯示。
- 原有 Game 整合涵蓋啟動、恢復、重置、單段／雙段進化、來源／目標缺失、排程拒絕、契約／存檔失敗、首錯資源及 fatal routing，全部通過。
- runtime_contract_loader、renderer_startup_error host 通過，git diff --check 通過。
- platformio run -e project_29 通過：Flash 56,944 / 65,536 B；靜態 RAM 6,860 / 20,480 B。本輪開始的同設定建置為 Flash 57,132 B、RAM 6,860 B；工作期間存在同目錄其他改動，兩次差異不視為本輪獨立 A/B 證據。
- 沿用已安裝工具鏈，未修改 platformio.ini；第三方 boolean deprecated 及 LTO serial compilation 警告保留。

未驗證：實機 SD／TFT／按鍵、設備時間與 SRAM 峰值、全旗標組態、完整 release gate。未 stage、commit 或燒錄。
