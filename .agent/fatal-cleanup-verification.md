# Fatal 收尾驗證（2026-10-06）

本批次疊加於原本未提交的 Session／外觀／Evolution 重構，沒有提交或回退其他修改。

## 實作

- `Game::enterFatalState()` 是唯一呼叫 `flow.enterFatalError()` 的 Game 路徑。它關閉 initialized、設定 RuntimeLoadState::Failed、清除 FirstStart 完成及 cheat Evolution 待辦、取消動畫與 Evolution、退出外觀選單並重置小遊戲；不呼叫 Renderer 顯示。
- prepare 失敗只收尾；finish 依 PetSession State 顯示既有診斷，保留 pet state／startup／首錯資源區分。執行期 `enterRuntimeFatal()` 收尾後顯示錯誤。
- loop 資源錯誤、Startup 缺少 FirstStart／排隊拒絕、FirstStart 播放失敗、播放資源錯誤、finish 畫面失敗及既有 Evolution／外觀 fatal 共用收尾。
- fatal 後公開執行入口由 initialized 阻擋，requestFullRedraw 也加入同一 gate；prepare／reset／finish 拒絕恢復 fatal Game。播放完成處理在 fatal 後立即停止，避免取消 FirstStart 後誤記完成。reset 的啟動播放發生 fatal 時回傳 false。

## 回歸證據

修改前新增的 `pwsh -NoProfile -File test/game_startup/run_host_test.ps1 -EnableAppearanceSelection` 失敗於 `!game.hasTransientAnimation()`：loop fatal 未取消排隊動畫。

最終通過：

- `pwsh -NoProfile -File test/game_startup/run_host_test.ps1`
- `pwsh -NoProfile -File test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation -EnableAppearanceSelection`
- `platformio run -c .pio/action-refactor.ini -e project_29`

新增案例覆蓋 loop 首錯、播放資源錯誤、實際啟動並切換的外觀選單、finish layout 失敗、FirstStart 缺失／排隊拒絕／非資源播放失敗、reset 啟動 fatal。fatal 後測試方向／確認鍵、立即與延後重繪、電池播放、loop、Startup、存檔、reset、cheat、prepare 及再次 finish，驗證沒有動畫、預覽、存檔、重載、Evolution 查詢或 Pet 變更。prepare 契約失敗與正常 prepare 均驗證沒有顯示錯誤。

同一工作區與 `.pio/action-refactor.ini` 修改前後：linked Flash 56,556 → 56,528 bytes（-28），靜態 RAM 6,860 → 6,860 bytes。這不是乾淨 HEAD 的比較。建置保留既有 Adafruit boolean 棄用及 LTO serial compilation 警告。

Host 使用真實 Game／AnimationController／PetSession／外觀選單／Evolution，Renderer、storage 與 SD 契約載入使用測試替身。小遊戲在上述 host 與 project_29 中關閉，未執行其 fatal 活動案例。實機 SD／TFT／按鍵、STOP 喚醒及 SRAM 峰值未驗收；尺寸與 host 結果不能代表實機表現。
