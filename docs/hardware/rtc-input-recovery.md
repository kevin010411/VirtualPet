# RTC、按鍵、背光與 STOP

現行實作整合 2026-10-09 RTC／STOP 與 2026-10-12 恢復修正。
`RtcClock` 使用 LSE/LXTAL，`SessionClock` 維持單調時間，`PetClock` 保留兩秒日邊界與餘數。
STOP 喚醒後分步恢復 RTC，不以同步等待阻塞普通按鍵。

RTC 不可用時用 millis 維持清醒進度並禁止 STOP；有效 counter 保留，
backup domain 失效才重建 epoch，避免巨量補時與清醒時間重算。
STOP 期間 counter 遺失的時間無法可靠還原；斷電／重啟沒有持久化時間 checkpoint。

普通單鍵首次輪詢按下即派送，沒有 30 ms 等待，保留 250 ms 短按間隔。
作弊單鍵仍於放開區分短／長按。左右導航可在補時期間更新；確認執行仍受
未完成補時／進化限制。提示音非阻塞。

無操作 30 秒降亮、120 秒請求 STOP；亮度受低電壓上限約束。
按住按鍵、暫態播放、待補算及 SD busy 阻止普通 STOP。
睡眠關背光／TFT／聲音／指示燈，按鍵喚醒後恢复時鐘与 RTC、等待放開並重畫，
不把喚醒按鍵直接當命令；錯誤畫面仍可降亮與睡眠。

喚醒後 TFT 退出睡眠固定等待 60 ms；進入睡眠前仍等待 120 ms。
每輪最多處理一個事件邊界：補算先依目前數值與 Daily Change 重算最短安全天數，
在條件範圍進入／離開、暫停結束或死亡門檻停下。無事件區間直接乘算並 clamp，
每段 backlog 的首日先逐日結算已成立的解鎖／取得條件，再開始批次。
邊界當天仍依原順序處理死亡、解鎖、圖鑑取得與進化；外觀切換後重新讀取條件。
進化播放／阻擋時保留 backlog；成功日交易消耗實際批次天數與原有兩秒餘數。
保存合併補算批次，事件保存保留。每日後處理預設 `1`，resolver 不輸出此開關；
只有診斷／尺寸比較才覆寫 `0`，詳見 [Profile flags](../profile_feature_flags.md)。

## 驗證

入口為 `test/rtc_clock`、`test/rtc_power`、`test/button_cheat_input` 與
`test/game_startup` 的 host scripts，覆蓋恢復、epoch／rollover、日餘數、補算、
30／120 秒與喚醒序號。過去通過記錄不是實機證據，完整套件需按當前源碼重跑。

實機尚待確認晶振起振／漂移、反覆 STOP、低電壓回升、真實 SD 補算與保存、
TFT 恢復、按鍵競態，以及電池端亮屏／降亮／STOP 電流。
仍以 STM32 framework 建置，不能據此宣稱 GD32 板已驗收。

2026-10-13 批次補算驗證：Daily Change 算術對照（6／10／12 Stats、正負極值、
暫停與長時間 clamp）、RTC 餘數／rollover、1000 天逐日與批次事件結果對照、
窄範圍連續進化／死亡、SD 解鎖邊界／讀取失敗，以及真實圖鑑單日取得範圍均通過。
`project_12` 建置通過，Flash 62040 B、靜態 RAM 7440 B。
完整 Game host 套件仍在 `testResetReloadFailure` 的 reloads／saves 斷言失敗；
修改前 HEAD 的隔離副本亦在同一斷言失敗。專項進化、死亡、補時與關閉每日後處理測試通過。
未燒錄或量測板上喚醒時間；60 ms TFT 等待仍待實機確認。
