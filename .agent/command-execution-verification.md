# 命令執行责任收斂驗證

日期：2026-10-06

## 實作

- CommandController 移除 CommandHost、can/execute 回呼與空的 executeUserAction；保留選取、可見性、命令身份與 Action Slot。
- CommandExecutor 改為一次 execute 回傳 CommandResult。每次結果均為區域變數，無前次執行結果殘留；Predict 的必要動畫檢查移至 executor。
- Game 確認鍵統一呼叫 executor，不再讀取 petBehaviorConfig.buttons[selectedSlot]。Action 結果仍觸發解鎖及基礎動畫更新；Outfit／小遊戲流程仍由 Game 處理。
- PetBehaviorRuntime 數值、原子提交及動畫規則未修改。未更動 platformio.ini，未 stage、commit 或燒錄。

## 通過的驗證

- game_startup host：既有啟動、恢復、重置、Evolution、fatal、Pet transaction 與 Status 回歸；新增統一入口的 hidden/inactive button、按鈕位置與 Action Slot 不同、有效 Action、無效 Action、缺動畫後保留值與 pause、下一次 System Command 不殘留 Action 結果；Game 確認鍵的正常／缺動畫情境驗證解鎖讀取提交值、錯誤呈現與無額外存檔。
- pet_behavior_runtime host：6 與 10 Stat 容量通過。
- animation_scene_playback host：通過。現行測試使用 PetStatSnapshot，補上明確 Pet.h 引用。
- custom_layout_export host：一般及 -Numeric 均通過。版面測試改直接驗證 selection visibility，命令派送由整合測試驗證。
- CommandExecutor.cpp 在 Guess Game／Outfit／Predict／Sequential Status 全開下以 g++ -Wall -Wextra 編譯成 object 通過；此項只是編譯檢查，未執行這些啟用分支。
- git diff --check 通過；靜態搜尋無 CommandHost、executeCurrent、executeUserAction、currentResult 或 begin/complete 舊協定使用者。
- 既有 .pio/action-refactor.ini 的 project_29／genericSTM32F103C8 release 完整連結通過。第三方 boolean deprecated 與 LTO serial 警告仍存在。

## 建置尺寸

同一工具鏈及設定，起始工作區 Flash 57,372 B、靜態 RAM 6,828 B；完成後目前工作區 Flash 56,956 B、靜態 RAM 6,828 B。期間存在其他並行播放／版面修改，因此 -416 B 是工作區比較，不能全部歸因於本批命令重構；也不是乾淨 HEAD 的比較。

## 未驗證

未驗證啟用的 Predict／Guess Game／Outfit 分支在設備上的行為，未做 GD32、實機 SD／TFT／按鍵驗收，未量測時間或 SRAM 峰值。
