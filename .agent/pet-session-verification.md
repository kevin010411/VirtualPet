# PetSession 責任收斂（2026-10-06）

## 實作

- `pet/PetSession` 擁有初始 Species／Outfit、存檔驗證與新建／恢復／重置政策，準備结果與失敗階段由 Session 回報。
- 新建重用初始已啟用契約、初始化解鎖並存檔；恢復僅在外觀不同時重載，保留存檔狀態並刷新解鎖；重置回到初始選擇，重載契約後初始化解鎖與存檔。
- `PetSessionHost` 只提供契約啟用與既有重載操作。Game 保留契約分配、畫面／輸入／播放協調、fatal 呈現與最終可執行狀態；`prepare_game()`／`finish_setup_game()` 仍為平台入口。
- Session 是 Game 的值成員，沒有新增動態配置。移除 Game 的初始選擇、準備結果、啟動錯誤暫存及不再使用的 PetStorage 參照。
- 準備結果表示 Pet 資料流程完成，並不表示顯示已初始化。重置以每次呼叫的 bool 結果控制 Game 是否恢復執行；沿用失敗不回復既有變更的規則。

## 驗證

- 修改前後 `test/game_startup/run_host_test.ps1` 通過。保留新建、同／不同外觀恢復、schema mismatch、無效預覽、重置、load/save failure、fatal routing、Evolution、Action、Status 與存檔節奏回歸。
- 同一 host 新增直接跨 `PetSession` 介面的測試，涵蓋六種成功／存檔退回新建情境、重置到初始狀態、七種失敗情境、未準備禁止重置、runtime failure 禁止重試及 Pet state failure 允許重新準備。
- `test/runtime_contract_loader/run_host_test.ps1`、`test/pet_persistence/run_host_test.ps1`、`test/renderer_startup_error/run_host_test.ps1` 通過。
- `git diff --check` 通過。
- 使用既有全域 PlatformIO 工具鏈，`platformio run -e project_29` 修改前後通過；未修改 `platformio.ini`。

| 同一 project_29 設定 | 修改前 | 修改後 | 差異 |
| --- | ---: | ---: | ---: |
| Linked Flash | 56,956 B | 57,132 B | +176 B |
| 靜態 RAM | 6,828 B | 6,860 B | +32 B |

新增固定依賴參照與 Session 介面有容量成本，沒有宣稱效能或記憶體改善。首次 target 編譯發現 SdFat 為 typedef 而不能使用 class forward declaration，已改讀正式 SdFat 標頭並完成最終建置。既有第三方 boolean deprecated 與 LTO serial compilation 警告保留。

未執行實機 SD／TFT／按鍵驗收、設備時間／SRAM 峰值量測或全旗標組態驗證。未 stage、commit、燒錄。
