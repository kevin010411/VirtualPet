# 編譯期 Profile

目前 `platformio.ini` 由 Web Profile Resolver 產生，預設專案 environment 為 `project_12`；`env:profile_base` 是其共用設定。專案環境的實際 flags 以該檔為準，不以舊客戶名稱或過去的範例 environment 推斷。

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e project_12
```

`/runtime.bin` 決定專案的 Pet Stats、User Actions、Status Sets、Evolution、初始外觀與 Command Layout。`include/shared/config/AppProfile.h` 提供編譯期 feature gate 與固定容量上限；`platformio.ini` 對 `project_12` 覆寫需要的值。關閉選單 slot 不代表對應程式碼一定離開 linked ELF，請以建置後的尺寸與符號確認。

`APP_PROFILE_*` 常數及 `ENABLE_COMMAND_SPECIES` 等舊相容入口仍存在於原始碼。它們不是目前 `project_12` 的產品設定；清理前須核對 Profile Resolver 的輸出與實際呼叫者，不能只依本文件刪除。現行 flag 請看 [Profile feature flags](profile_feature_flags.md)，尺寸驗證請看 [Profile size 驗證](profile_size_verification.md)。
