# RTC、STOP 與背光實作驗證

日期：2026-10-09。韌體修改完成；未燒錄、未量測實機。

## 實作

- `platform/RtcClock` 使用現有 F1 CMSIS 寄存器名稱開啟 32.768 kHz LSE/LXTAL，RTC 分頻 32767、每秒累加，停用 RTC 秒／alarm 中斷。有效備援標記、時鐘來源及分頻吻合時保留 counter；STOP 喚醒只同步寄存器。不接受靜默 RC fallback。起振最多等待 5 秒，同步／寫入最多 1 秒；失敗顯示 `RTC clock error` 並停止 Game。
- `pet/PetClock` 保留 2 秒邊界及餘數，使用無號差值處理 counter rollover。Game 每輪最多嘗試一天；動畫／Evolution 阻擋時保留邊界，每日交易開始後的失敗會消耗該邊界，避免重複套用。完成原快照後再納入補算期間經過的時間，未追上前阻止新輸入。
- `PetSaveController` 合併补算的週期性保存；一小時無進化／解鎖變更的情境，1,800 天只增加一次週期性存檔嘗試。既有外觀與生命週期的立即保存仍照常。失敗不回滾 RAM 進度；週期計數飽和後保留重試資格。
- `platform/PowerController` 共用最後互動基準：30 秒 PWM 64、120 秒請求 STOP。亮度取無操作設定與既有電壓上限的較小值；按住按鍵、暫態動畫、待補算或 SD busy 阻止一般 STOP，不重設動畫造成的無操作時間。
- STOP 關閉背光、TFT 顯示／休眠、蜂鳴器及 PA3 指示燈。按鍵 ISR 只增加喚醒序號，遮蔽中斷後再檢查序號與按鍵、執行 WFI，不清除晚到的按鍵事件。非按鍵中斷保持暗屏再睡，低電壓按鍵喚醒保留暗屏恢復檢查。
- 喚醒重設系統時鐘／SysTick、恢復 PVD、檢查 RTC 與補算、等待按鍵放開，再重畫。一般補算期間保持背光關閉；需要播放既有暫態進化段落時允許受電壓限制的畫面，仍阻止輸入。錯誤畫面也會降亮及睡眠，醒來恢復原診斷。
- TFT cheat 重啟改為只初始化及重画 TFT，不重載 Pet Session、不重置養成基準。真正 reset Pet 建立新的基準。

權威 Pet 規則未修改，仍見 `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`。這裡只記錄韌體整合與驗證。

## 已執行檢查

| 命令 | 結果／覆蓋 |
| --- | --- |
| `pwsh -NoProfile -File test/rtc_clock/run_host_test.ps1` | 通過；寄存器模擬：初始化、保留 counter、來源／分頻錯誤、起振與同步逾時。不是實際晶振驗證 |
| `pwsh -NoProfile -File test/rtc_power/run_host_test.ps1` | 通過；2 秒邊界、餘數、重複快照、32-bit rollover、30／120 秒、電壓上限及睡眠阻擋 |
| `pwsh -NoProfile -File test/game_startup/run_host_test.ps1` | 通過；新增一小時 1,800 天、單次批次保存、保存失敗、解鎖提交後錯誤不重算、補算阻止 Action、動畫保留邊界、補算期間新增時間、重畫保留餘數與 Pet reset |
| 同上加 `-EnableFirstStartAnimation -EnableAppearanceSelection` | 通過；保留 FirstStart 與換裝回歸，動畫仍使用毫秒時基 |
| `pwsh -NoProfile -File test/button_cheat_input/run_host_test.ps1` | 通過；既有短／長按及 cheat，加按住判斷、喚醒序號不被手勢清除 |
| `pwsh -NoProfile -File test/pet_behavior_runtime/run_host_test.ps1` | 通過；Stats=6／10，Action、原子交易、Daily Change 暫停 |
| `pwsh -NoProfile -File test/pet_persistence/run_host_test.ps1` | 通過；既有持久化回歸 |
| `pio run -e project_29` | 通過；Flash 58,168 / 65,536 B，靜態 RAM 6,876 / 20,480 B |

建置仍是 `ststm32`／`genericSTM32F103C8`／Arduino；未遷移框架、未擴張 Flash 設定。GD32F103CB 的 LXTAL／RTC register 對照依據為 [GD32F10x 官方手冊](https://www.gd32mcu.com/data/documents/userManual/GD32F10x_User_Manual_Rev2.9.pdf)。編譯與寄存器模擬不能證明這塊 GD32 實機與目前 STM32 framework 的完整相容性。建置僅有依賴庫 `boolean` 棄用及 LTO 平行度警告。

## 尚未驗收

1. 實物 Y2 起振、RTC 每秒增加、重複初始化與 STOP 前後 counter 持續、一小時漂移。
2. 30 秒降亮、120 秒暗屏 STOP、三鍵喚醒不誤操作、長按與睡眠邊界競態。
3. 一小時睡醒後含真實 SD／Evolution 的補算時間、存檔次數、畫面恢復及操作恢復時間。
4. 低電壓暗屏重入、回升後不閃全亮、TFT／TF 恢復及卡 busy 狀態。
5. 電池端亮屏／降亮／STOP 電流。共用 3.3 V 未切斷，不代表整板僅 RTC 耗電。

第一版時間邊界只保留於 SRAM，保证持續供電同一執行期的 STOP 連續性；重開機、拔主電池與 brownout 仍不保證追補先前經過時間。尚未加入鈕扣電池或與 Pet 狀態一致的持久化時間 checkpoint。
