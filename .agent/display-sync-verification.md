# 畫面同步責任收斂（2026-10-06）

沿用尚未提交的目錄整理與既有重構，未 stage、commit、燒錄或修改 platformio.ini。修改前任務檔案保存在 `.pio/display-sync-before/`。

- `LayoutRenderer::configureRuntimeContract` 擁有清除舊動畫區域、掃描 screen blocks、設定動畫矩形及數值 fallback。無動畫積木時維持零區域；重新設定契約仍使版面失效。
- `LayoutRenderer::syncPlayback(snapshot)` 擁有目前 layout ID 查詢、`asset data` 錯誤記錄、數值更新與版面同步。Game 保留 prepare → sync → tick 及原 PlaybackFailed 分流；移除 Game 的同步 helper。
- Game startup host 修改前後通過。Animation Scene playback host 通過，驗證版面先於動畫首幀、完成、排隊、中斷及 Idle fallback。
- Custom layout export host 通過 moved、enlarged、shrunk-duplicates、animation-only、buttons-only、empty 六種版面；Numeric host 通過數值更新、fallback、外觀重配及完整重繪。测试改走正式同步入口，不自行設定動畫區域。
- 同一 `platformio run -e project_29` 修改前 Flash 56,972 B、靜態 RAM 6,828 B；修改後 Flash 56,956 B（-16 B）、靜態 RAM 6,828 B（不變）。已安裝全域工具鏈完成兩次建置；工作區 `.piohome` 未安裝平台。
- 未驗證實機 SD／TFT／按鍵、設備時間、SRAM 峰值或全旗標組態；不宣稱效能改善。既有第三方 boolean deprecated 及 LTO serial compilation 警告保留。
