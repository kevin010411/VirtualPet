# Game 播放後處理與方向鍵收斂（2026-10-06）

- `completePlaybackTick` 為 Game 私有操作，供 `loop_game` 與 `redrawAllNow` 共用。順序保留：播放失敗處理、FirstStart 完成、Accepted 時推進 Evolution、最後檢查 fatal。兩個入口保留原有錯誤顯示差異。
- `routeDirectionKey` 與私有 `KeyDirection` 集中左右鍵路由；保留未初始化／Evolution 阻擋、外觀選單、小遊戲、Command 的優先順序及 dirtySelect 更新。沒有新增 class、狀態成員或配置。
- 既有 Game host 測試通過：`test/game_startup/run_host_test.ps1`，以及同一命令加 `-EnableFirstStartAnimation`。未新增 private helper 結構測試。
- 前後均以 `platformio run -c .pio/action-refactor.ini -e project_29` 建置成功，使用既有 genericSTM32F103C8 比較設定，未修改 platformio.ini。Flash 56,944 → 56,956 bytes（+12）；靜態 RAM 6,860 → 6,860 bytes。仍有第三方 boolean deprecated 與 LTO serial compilation 警告。
- `git diff --check` 通過。保留本次開始前的工作區變更；尺寸基線包含那些變更。
- 未執行完整 release 驗證或實機 SD／TFT／按鍵驗收；目前 profile 及 host 都關閉小遊戲，其方向鍵分支僅靜態核對。
