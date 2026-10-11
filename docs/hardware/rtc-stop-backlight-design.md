# RTC、STOP 與背光設計草案

日期：2026-10-09。狀態：已依確認參數修改韌體，受影響 host 與 `project_29` build 通過；實機驗收未完成。實作與檢查結果見 [RTC／STOP 驗證紀錄](rtc-stop-backlight-verification.md)。下方現況段落保留修改前的設計依據。

## 目標與範圍

使用 GD32F103CB、現有 Y2 32.768 kHz 晶振及 3 顆 1.5 V AAA 鹼性電池，讓持續供電的同一執行期在 STOP 睡眠期間保留經過時間，恢復後不漏算養成；無操作先降背光，再睡眠。

硬體基準見 [電源策略評估](power-strategy-2026-10-09.md) 與 [原理圖](STM32_1.8寸屏_TF.pdf)。本階段不切斷共用 3.3 V，不加入鈕扣電池，不改 TFT／TF 的供電硬體。

Pet Stat、每日推進、Evolution、存檔的權威規格仍位於 `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`；本文件只描述韌體計時與平台整合，不取代跨層產品規格。養成週期變更須在權威規格中記錄後再實作，不能只由 firmware 私自定義。

## 修改前現況與已確認參數

- `include/controller/Game.h:69`：`gameTick = 2000` ms。
- `src/controller/Game.cpp:69-82`：每次間隔到達只呼叫一次 `maybeTickPet()`，並把基準設為當下 `millis()`，沒有補算全部經過週期。
- `src/controller/Game.cpp:445-474`：動畫／Evolution 可能阻止這次每日推進；`maybeTickPet()` 沒有向呼叫端回報「已推進／忙碌／失敗」。
- `src/controller/CommandExecutor.cpp:136-137`：其自身的 `gameTick` 也用在動畫時間，不能全專案把同名值改成 24 小時。
- `src/platform/main.cpp:699-761`：先派送按鍵，再執行 Game；接入 RTC 後須先处理已到期的養成，再允許新的修改型 Action。
- 現有 60 秒無操作睡眠、Normal/Dimmed/Critical PWM 255/128/26、5 秒電壓恢復確認與漸進增亮。
- `ButtonInput` 已設定 PA10/PA11/PA12 falling EXTI，`clearFlags()` 已含等待按鍵放開邏輯。
- `PetSaveController` 每兩次 `maybeSave()` 嘗試存檔，成功後才清除計數；直接用它補算上千週期會造成大量寫入。

**使用者已確認：養成一天維持 2 秒，無操作 30 秒降亮，無操作 120 秒關閉背光並進入 STOP，由 RTC 持續計時。** `petDaySeconds = 2`，不改成現實 24 小時一天，也不連帶變更動畫的毫秒時基。

| 已確認項目 | 設定 | 結果 |
| --- | --- | --- |
| 養成日 | 2 秒 | 睡一小時到期 1,800 個週期，依既有上限與每日規則逐步處理 |
| 無操作降亮 | 30 秒 | 背光降亮，畫面與養成持續執行 |
| 無操作睡眠 | 120 秒 | 背光 0、TFT 休眠、CPU STOP；RTC 計數不中斷，醒來補算 |

「只剩 RTC 計時」指不讓 CPU 週期性執行養成／渲染或寫 TF。現有共用 3.3 V 不會切斷，SRAM、喚醒電路與外設仍有供電／靜態耗電，不代表整板僅 RTC 耗電。

## 模組分工與介面

| 模組 | 小介面 | 實作責任 |
| --- | --- | --- |
| `platform/RtcClock` | `begin()`、`readSeconds(out)` | 備援域存取、LXTAL 起振、RTC 1 Hz 計數、有效性與一致讀取；回傳明確錯誤 |
| `platform/PowerController` | 輸入當下 ms、電壓狀態、互動／睡眠阻擋條件，產生背光目標與睡眠請求 | 無操作狀態、背光上限合成、恢復時機；不操作 Pet Stats |
| Game 內的養成計時狀態（必要時作 `pet/PetClock` 值成員） | 開始／重置時間基準、接收 RTC 秒數、取得下一個到期邊界、消耗已處理邊界 | 日期差、餘數、待處理週期；不依賴 HAL、SD 或 TFT |
| 既有 Game／PetBehaviorRuntime／Evolution／PetSaveController | 延用實際推進、播放與存檔操作；Game 新增明確的時間同步結果 | 決定是否可執行一天、保留進化順序、控制輸入和存檔 |

不新增虛擬 clock 層、heap 物件或通用 timer framework。主迴圈把可測試的數值與事件傳入；RTC 寄存器細節不進入 Game。平台直接序列化操作 TFT、STOP 與恢復，不把硬體生命週期藏成散落的 bool 回呼。

## RTC 持續計時

1. 啟動時開啟需要的電源／備援域存取，檢查有效性標記、時鐘來源與 RTC 設定。
2. 未初始化時啟動 LXTAL，使用有上限的起振等待。選定 LXTAL 後設定 RTC 為每秒累加一次（32.768 kHz 對應 divide-by-32768；具體寄存器編碼以 GD32 手冊為準），計數可以從 0 開始。
3. 已有效時保留計數与分頻，不在每次開機／喚醒 reset backup domain。停用不使用的 RTC 秒中斷、overflow 中斷和 alarm；RTC 計數本身照常運作。
4. 讀取多段 counter 要取得一致值；喚醒／APB 恢復後完成必要同步。所有等待都能回報失敗，不能無限阻塞。
5. 起振、初始化或讀取失敗時回報 `RTC clock error`／對應狀態，阻止未驗證的時間推進。第一版不靜默改用 SysTick 或內部 RC 冒充準確計時；沿用短錯誤、需重新開機的處理方式。
6. 用無號差值處理 32-bit counter wrap，並定義可接受的讀取間隔與時間失效情況。正常 STOP 幾小時不重設 baseline；非法來源改變／備援域重設則視為時間失效，不解讀成巨大的睡眠時間。

RTC 追蹤的是經過秒數，第一版不用日期設定 UI、時區或網路校時。它也不保證石英零誤差；實機先驗證起振及漂移。未來加入日期時，調整日曆不得直接改變養成的單調時間基準。

## 養成時間與補算

正常互動與睡眠後使用同一個 RTC 基準，不把睡眠秒數加到 `millis()`。按鍵 debounce、PWM 漸亮、小遊戲與動畫繼續使用原本毫秒時基，避免喚醒後所有 UI timer 突然到期。

設 `lastBoundary` 為最後已消耗的 RTC 邊界，`nowSeconds` 為一致讀取的 RTC：

```text
due = unsigned(nowSeconds - lastBoundary) / petDaySeconds
每消耗一個養成邊界：lastBoundary += petDaySeconds
餘數自然保留；禁止每次把 lastBoundary 直接改成 nowSeconds
```

第一次進入可操作 Pet Session 時建立基準；Startup／FirstStart 之前不補算啟動耗時。重置寵物成功時建立新的 baseline；重畫 TFT、換裝與一般按鍵不重設 baseline。以上為待採納的 Session 時間政策。

Game 必須區分 `Blocked`、`Consumed`、`ConsumedWithError`、`Fatal`，不能把「有呼叫 maybeTickPet」當成完成一天：

- `Blocked`：暫態動畫／既有 Evolution 流程阻擋提交，保留全部到期時間，繼續播放直到可處理。
- `Consumed`：這次邊界已完成每日規則，消耗恰好一個週期；後續畫面或解鎖錯誤不能讓下一輪重複提交。
- `ConsumedWithError`：每日交易已開始但提交失敗，既有 Daily Change 暫停計數可能已被消耗；依權威規格不得重試同一邊界而重複遞減，沿用明確失敗處理，不宣稱資料已提交。
- `Fatal`：停止輸入、推進、播放與存檔，沿用現有 fatal 收尾。

不能直接把最終值套成 `DailyChange × due`：逐日暫停計數、clamp、物種切換与規則重載都可能影響結果。先以逐日順序為正確性基準，Evolution 阻擋時保持未處理邊界。是否省略過去進化動畫、只顯示最後狀態是另一個產品選項，本草案不直接採用。

每個主迴圈最多執行少量推進工作；不得用一次性無界 while 堵住按鍵、播放或低電壓檢查。時間同步開始時可固定一個目標 RTC 快照，先處理到該快照；處理期間新經過的時間留到下一輪。2 秒已確認為養成速度；若大量進化造成長期追不上，需評估補算呈現策略与工作量，不能擅自改慢養成速度或丟棄到期天數。驗收至少包含睡一小時後 1,800 個週期的補算與存檔次數／可操作恢復時間。

到期的邊界先於新 Action 處理，防止睡了很久後先餵食再套過去的變化。補算未完成時暫停修改型輸入，也不執行 cheat reset 等變更；等待按鍵放開，不排隊重放按鍵。fatal／低電壓可以中止處理而保留未消耗時間。

存檔由既有責任持有者處理：新增「補算批次」可合併可合併的週期性保存，不每補一天寫一次。物種／Outfit／FirstStart 等現有必須立即保存的生命週期照舊；批次完成、電壓允許時保存更新結果。存檔失敗不能把 RAM 中已推進的天數回滾或重複套用。

第一版 RTC baseline／未滿一天餘數保留在主 SRAM，STOP 會保留它們。本階段保證正常睡醒的連續性，**不保證重啟、拔電池或 brownout 後從原時間接續**；後續需把時間 checkpoint 与寵物状态一致保存並設計版本處理，不能只新增一個不相關的 timestamp 檔。Daily Change 暫停計數仍遵守既有重啟清除規則。

## 背光與電源狀態

時間參數已確認：無操作 30 秒降亮、120 秒睡眠。降亮 PWM 64、互動時保留現有 PWM 255 是本設計建議的亮度值；使用者未另指定亮度，數值不代表已量測到耗電比例。

| 狀態 | 進入條件 | 行為 |
| --- | --- | --- |
| Active | 啟動完成、有效互動、按鍵喚醒且電壓允許 | 正常畫面、背光以電壓上限限制 |
| IdleDim | 無操作 ≥30 秒 | 畫面繼續運作，背光目標 64 |
| Stop | 無操作 ≥120 秒，沒有按住按鍵、補算／暫態播放／SD 寫入等睡眠阻擋 | 背光 0、TFT sleep、蜂鳴器關閉、CPU STOP、RTC 持續 |
| Resume | 按鍵 EXTI 喚醒 | 電壓檢查、時間同步、TFT 重畫、消耗喚醒鍵，最後漸亮 |
| Critical | 沿用現有低電壓條件 | 優先降載；背光最多 26，持續低壓後 STOP，醒來不足以恢復則保持暗屏再睡 |

背光統一計算：`target = min(無操作狀態要求, 電壓狀態上限)`。例如 IdleDim=64、電壓 Dimmed=128，最終 64；即使按鍵使 Active=255，電壓 Dimmed 也最多 128。Stop／未完成恢復一律 0。降亮立即，增亮沿用目前分步 ramp，不讓互動覆蓋低電壓限制。

無操作時間從最後有效的使用者互動計算；30 秒及 120 秒共用同一基準，降亮後再經過 90 秒便達到睡眠門檻。暫態與 base/idle 動畫不重設互動時間；不可中斷的暫態播放或 SD 寫入可延後真正的 STOP，安全工作完成且已超時時立即睡眠，不重新等 120 秒。按鍵一旦 debounced 按下就可恢復亮度／禁止睡眠，正式 Action 照既有放開後派送；避免按住 120 秒後睡下但沒有新的 falling edge 可喚醒。

錯誤畫面也要有獨立的降亮／STOP 流程，且醒來只恢復診斷畫面，不讓 fatal Game 恢復執行。不能因 `g_sdReady/g_gameReady` 的早退讓錯誤畫面永久全亮；這需作為平台流程整理的一部分。

## STOP 進出順序

進入：确认沒有按鍵按住／未完成 SPI／SD 寫入或不可中斷播放 → 結束可完成的時間同步 → 背光 0、PA4 低、PA3 LED 不導通 → TFT display off／sleep in（依模組規格等待命令完成）→ 完成 SD flush／busy，SPI idle → 設定安全 GPIO → 處理既有 EXTI pending 與最後按鍵狀態 → 暫停 SysTick、停用 PVD → 低功耗 STOP/WFI。

進入睡眠的最後檢查與 WFI 必須避免按鍵競態；不能在最後一刻無條件清掉剛到的按鍵中斷。ISR 只記錄喚醒原因，不操作 TFT、SD 或 Pet。PB0 高阻態問題與各 GPIO 仍需實物確認；不一律改 analog，PC14/PC15 必須保留 RTC 晶振功能。

恢復：確認喚醒來源 → 恢復系統時鐘／APB／SysTick／PVD → RTC 一致讀取 → 低電壓檢查，必要時保持暗屏再 STOP → 清除按鍵手勢並等待放開 → TFT sleep out 並等待现有 120 ms → 同步到期養成，保持畫面更新可用與背光受控 → 清屏／完整重畫 → display on → 漸進恢復到合成背光目標 → Active。

補算若觸發可見 Evolution，進入受控恢復流程：先同步當前畫面、在安全亮度播放既有段落，期間不接受修改型輸入；完成後繼續處理剩餘天數。最終完成前不能先以睡前舊 Pet 畫面作為可操作狀態。

第一版不開 RTC 每秒喚醒或每日 alarm，數值在按鍵喚醒後補算。這滿足養成結果連續，但睡眠期間不会即時播放進化或提示。未來若需要準時事件，再加入 RTC alarm 的暗屏喚醒路徑。

## 實作批次與驗收

1. **RTC 平台**：先核對 GD32 與目前 STM32 Arduino/HAL 的實際寄存器、時鐘與 build 支援；僅增加 RTC 初始化／讀取，測起振、STOP 前後秒數、重複 begin 不重設。若不相容，先提出最小平台處理，不能順帶遷移整個 framework。
2. **無操作背光**：加入純狀態計算與統一目標，將一般無操作 STOP 改為 120 秒，保留低電壓與喚醒鍵處理。驗證 30/120 秒邊界、暫態動畫、按住按鍵、fault 畫面与降壓恢復。
3. **養成時間整合**：採用已確認的 petDaySeconds=2，以 RTC 排程、逐日回報消耗結果、先同步再派送輸入、合併批次存檔。UI 毫秒時基維持；暫停計數／Evolution 保持權威契約。
4. **STOP 恢復整合**：驗證按鍵競態、安全 GPIO、低壓重入、完整重畫與時間同步的順序。

需有意義的 host 測試：跨邊界餘數、重複讀取不重算、32-bit wrap、Blocked 保留到期日、失敗不重複消耗 pause、Action 到期前後順序、進化阻擋、批次保存與失敗、背光上限優先序。依變更執行受影響的 PetBehavior、Game、ButtonInput／存檔 hosts 及實際 profile build，記錄 Flash/RAM，不預設跑全套跨 repo 驗證。

實機另外驗收：LXTAL 起振與 1 小時漂移、睡眠跨多個週期、三鍵醒來且不誤執行、低壓醒來不閃全亮、長按不困在 STOP、SD 讀寫／恢復、電池端亮屏／降亮／STOP 电流。沒有實測不能宣稱續航改善比例或 GD32 與 STM32 完全相容。

## 參考

- [GD32F10x User Manual Rev 2.9](https://www.gd32mcu.com/data/documents/userManual/GD32F10x_User_Manual_Rev2.9.pdf)：備援域、RTC、第 3 章省電模式；GD32 稱 Deep-sleep，本文 STOP 指目前韌體的同類深度睡眠流程。
- [GD32F103xx Datasheet Rev 2.15](https://www.gd32mcu.com/data/documents/datasheet/GD32F103xx_Datasheet_Rev2.15.pdf)：實際器件 RTC／供電與電流條件。
- 現有 `docs/architecture.md`、`include/controller/Game.h`、`src/controller/Game.cpp`、`src/controller/GameStartup.cpp`、`src/pet/PetBehaviorRuntime.cpp`、`src/pet/PetSaveController.cpp`、`src/platform/main.cpp`、`src/platform/ButtonInput.cpp`。圖譜部分行號過期，關鍵流程已直接核對當日檔案。
