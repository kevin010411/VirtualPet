# 編譯期 Profile

目前 `platformio.ini` 由 Web Profile Resolver 產生，預設專案 environment 為 `project_12`；`env:profile_base` 是其共用設定。專案環境的實際 flags 以該檔為準，不以舊客戶名稱或過去的範例 environment 推斷。

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e project_12
```

`/runtime.bin` 決定專案的 Pet Stats、User Actions、Status Sets、Evolution、初始外觀與 Command Layout。`include/shared/config/AppProfile.h` 提供編譯期 feature gate 與固定容量上限；`platformio.ini` 對 `project_12` 覆寫需要的值。關閉選單 slot 不代表對應程式碼一定離開 linked ELF，請以建置後的尺寸與符號確認。

舊客戶名稱的 `APP_PROFILE_*` 巨集與直接切換 Species 的 `ENABLE_COMMAND_SPECIES` 路徑已移除。Web Profile Resolver 不輸出這些旗標；Species 切換仍由 Evolution 與首次啟動選擇流程處理。`change_species` 的 Runtime Command ID 保留作為不接受的 tombstone，不能重新指定給其他命令。現行 flag 請看 [Profile feature flags](profile_feature_flags.md)，尺寸驗證請看 [Profile size 驗證](profile_size_verification.md)。
