# FirstLaunch 選擇流程清理（2026-10-06）

- `setRendererAssetAppearance` 在韌體中只有宣告與定義，Web graph-augmented search 未找到依賴；移除這個會載入契約並直接修改 Pet 的公開入口。
- `isFirstLaunchSelectionPending` 固定回傳 false，啟動完成直接進入 Command；移除其不可達選擇流程及 `completeFirstLaunch` 的回傳／呼叫分支。
- 移除 AppFlowController 的 FirstLaunch stage、選擇所需命令設定與操作；保留其餘 AppStage 原本數值。
- 保留 FirstStart 的動畫、完成紀錄、失敗处理與重置。Pet flowFlags、既有 FirstLaunch flag 存取器及 save version 不變，沒有持久化格式變更。

驗證：

- `test/game_startup/run_host_test.ps1` 通過。
- `test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation` 通過；新增成功完成／存檔失敗／播放失敗／缺動畫、恢復保留完成紀錄、重置清除後再次播放存檔，以及 Startup 結束後可執行命令的覆蓋。既有 Evolution 中斷／重置測試加入啟用動畫時的額外完成存檔預期。
- `test/pet_persistence/run_host_test.ps1` 通過。
- Game、GameStartup、AppFlowController 在 Guess Game、Outfit、Predict、Appearance Selection、Startup 與 FirstStart 全部啟用且 Debug 關閉的 host 標頭組態通過 `g++ -fsyntax-only`；這是語法檢查，不是該組態的完整連結或執行驗證。
- `platformio run -e project_29` 通過；Flash 56,944 / 65,536 B，靜態 RAM 6,860 / 20,480 B。project_29 未啟用 Startup／FirstStart；啟用分支以 host 測試驗證。
- 本輪開始的同設定建置為 Flash 57,132 B、RAM 6,860 B；另一個 chat 同時修改外觀／Evolution／Session 的結果介面，因此差異不作為本次清理的獨立 A/B 數據。
- `git diff --check` 通過。未修改 platformio.ini，沿用既有工具鏈。

未執行實機 SD／TFT／按鍵驗收、設備時間／SRAM 峰值量測或完整 release gate。未 stage、commit 或燒錄。
