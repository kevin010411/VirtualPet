# 韌體 host 測試

使用 PowerShell 7 和可從 PATH 執行的 Windows MinGW-w64 `g++`（C++17）。測試在
韌體根目錄的 `.pio/` 產生並執行 Windows 測試程式。各案例依序跑，第一個失敗會傳回
非零退出碼。同一 checkout 的 runner 共用輸出路徑，請依序執行。

需要 Web fixture 時，使用配對的 [Web checkout](../docs/README.md#跨端規格)。
預設位置為韌體的 `../../web`。產生 fixture 須安裝 Web 的 Python 環境，並通過
Web 的 `scripts/preflight.ps1 -BackendOnly`。

```powershell
.\test\run_host_tests.ps1 -List
.\test\run_host_tests.ps1 -Case pet-behavior,stat-event-rules
.\test\run_host_tests.ps1 -Case startup -RunnerOptions @{ EvolutionOnly = $true }
.\test\run_host_tests.ps1 -Case shop -RunnerOptions @{ FixtureRoot = '../path/to/shop/list/card' }
```

`-RunnerOptions` 傳遞單一案例的原有 PowerShell 參數。下表列出常用入口，
完整名稱及對應腳本以 `-List` 為準。腳本的 `param` 列出可用旗標。

| 功能 | `-Case` | 選項或資料 |
| --- | --- | --- |
| 寵物規則、存檔、每日結算 | `pet-behavior`, `pet-persistence`, `runtime-contract` | 內建測試資料 |
| 啟動、狀態、進化、死亡 | `startup` | `StatusOnly`、`EvolutionOnly`、`DeathOnly`、`DeathEnabled` |
| Runtime Table 與 Web 資源 | `runtime-table`, `animation`, `custom-layout`, `layout-media` | Web 已追蹤與下方可再生的 fixture。Runtime Table 可用 `WebFixturesRoot` 覆寫根目錄 |
| 圖鑑、卡片、猜物、手動日結算 | `collection`, `collection-cards`, `guess`, `manual-day` | `collection` 預設使用 RTC，並讀 Web 的 `collection_catalog_v10/valid`。特殊案例使用 `FixtureRoot`。基本圖鑑額外支援容量、恢復與啟用案例 |
| 商店與交易 | `shop`, `transactions` | 商店分別跑 `single` 和 `list` fixture。交易案例使用內建資料 |
| 抓取遊戲 | `capture` | 下方產生的 v32 fixture，含首次捕獲圖卡和逆序圖鑑依賴 |
| 專案選單 | `project-menus` | 下方產生的 v29 fixture，含卡片、圖鑑和巢狀選單 |
| 數值事件 | `stat-event-rules`, `stat-events` | 規則案例使用內建資料。整合案例需要符合韌體案例資料的兵兵役期 SD 套件 |
| 口令反應、動畫音訊 | `instruction-response`, `sequence-audio` | 使用 Web 的 `instruction_response_v23`、`sequence_audio` fixture。口令反應可單獨跑內建規則 |
| 按鍵、硬體時鐘、電源、SD 與繪圖 | `button-input`, `rtc-clock`, `rtc-power`, `storage-sd`, `bundle-reader`, `startup-error-rendering` | Host 替身。依需求使用 `Gd32Dma`、`ProjectMenus` 等選項 |

數值事件整合測試使用 37 筆圖鑑、13 次日結算後的退伍分支及第 3 天分發條件。
請提供該版本的解壓 SD 套件。此案例同時啟用專案選單，以載入該套件的選單區段。
測試所用的按鈕、數值槽位與結局素材見
[`verifyBingbingEvents`](collection_catalog/test_main.cpp)。
圖鑑、商店、抓取、專案選單與數值事件的整合執行均沿用
[`collection_catalog` runner](collection_catalog/run_host_test.ps1)。

## 取得可重跑的 Web fixture

在 Web repo 根目錄執行所需指令。使用同一個配對 checkout 產生和檢查資料。

```powershell
# Runtime Table、口令反應、版面與素材包
pwsh -NoProfile -NonInteractive -File scripts/generate_screen_layout_fixtures.ps1

# 圖鑑卡片。其他案例可用產生器的 --help 查詢模式。
.\backend\.venv\Scripts\python.exe -m scripts.generate_collection_catalog_fixtures --cards --output .verification/firmware-host/cards

# 抓取、專案選單
.\backend\.venv\Scripts\python.exe -m scripts.generate_firmware_integration_fixtures capture .verification/firmware-host/capture
.\backend\.venv\Scripts\python.exe -m scripts.generate_firmware_integration_fixtures project-menus .verification/firmware-host/project-menus

# 商店單項與列表
$env:DREAMOLI_SHOP_TEST_FIXTURE = Join-Path $PWD '.verification/firmware-host/shop'
try {
    .\backend\.venv\Scripts\python.exe -m unittest tests.pet_editor.test_shop.ShopTests.test_sd_export_carries_shop_commands_native_art_and_transactions
} finally {
    Remove-Item Env:DREAMOLI_SHOP_TEST_FIXTURE
}
foreach ($presentation in 'single', 'list') {
    $shopFixture = ".verification/firmware-host/shop/$presentation"
    Expand-Archive "$shopFixture/sd-card.zip" "$shopFixture/card" -Force
}
```

回到韌體 repo，將解壓套件目錄交給案例。商店指定 `shop/single/card` 或 `shop/list/card`。

```powershell
$fixtures = '../../web/.verification/firmware-host'
.\test\run_host_tests.ps1 -Case capture -RunnerOptions @{ FixtureRoot = "$fixtures/capture" }
.\test\run_host_tests.ps1 -Case project-menus -RunnerOptions @{ FixtureRoot = "$fixtures/project-menus" }
```

硬體驗收步驟見[開發流程](../docs/development-workflow.md)。
