# 死亡執行

`ENABLE_DEATH` 預設 `0`，resolver 依 Project `death.enabled` 輸出。
開啟後，Pet 在原子 Stat 提交後判斷死亡，Game 取消 Startup／Evolution／
圖鑑／手動外觀／小遊戲工作，以死亡基礎動畫顯示並鎖定普通命令。
死亡保存失敗保留 RAM 的 Dead，每秒最多重試一次；低電壓仍可停止迴圈。

關閉時不編入新死亡判定、死亡動畫解析及新死亡保存重試。
已死亡存檔仍鎖定操作、顯示 default idle，允許新按鍵重開；成功後不再死亡。
啟用死亡的 SD 契約需匹配編譯能力。

新局使用初始外觀契約建立狀態、重算初始 grants，保存最終快照後才解除 Dead。
保存失敗恢復原 Pet／外觀，仍可按鍵重試；必要資源失敗走 fatal。
成功會清除日暫停、補時 backlog、保存批次、選單及暫態工作。
存檔保持 v18／104 bytes，切旗標不變更 fingerprint 或二進位格式。

驗證入口：`test/game_startup/run_host_test.ps1 -DeathOnly -DeathEnabled 0`／`1`、
Pet persistence 與 Runtime Table hosts。實機 SD 中斷寫入及 TFT／按鍵另驗收。
