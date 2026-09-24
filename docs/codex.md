# 建置

目前 `platformio.ini` 預設建置 `project_12`：

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e project_12
```

專案環境由 Web Profile Resolver 產生。建置其他專案前，先確認其 environment 已存在於目前的 `platformio.ini`；不要使用舊文件中的 `default`、`kuromu`、`small` 等名稱。

- [架構與分階段精簡方案](architecture.md)
- [編譯期 Profile](app_profiles.md)
- [Profile flags](profile_feature_flags.md)
- [Profile size 驗證](profile_size_verification.md)
- [完整文件索引](README.md)
