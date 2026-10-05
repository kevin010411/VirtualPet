# EvolutionController 抽離驗證（2026-10-06）

本批沿用使用者尚未提交的九類目錄整理；未 stage、commit、燒錄或修改 `platformio.ini`。
重構前檔案與 ELF 保存於忽略目錄 `.pio/evolution-before/`。

## 責任與介面

- `appearance/EvolutionController` 擁有 Evolution 查詢、階段與目標資料、兩次來源／目標播放、外觀轉換時機、播放失敗處理及基礎動畫交接。
- Game 對控制器只有 `check`、`update`、`cancel`、`isActive` 四個操作；沒有階段 getter、目標 getter 或依階段處理失敗的分支。
- `EvolutionHost` 使用 Game 原有的外觀套用與基礎動畫更新方法；契約重載、解鎖初始化、Pet 外觀／stage_days 更新及單次存檔沿用既有順序。
- 查詢失敗仍使 Game 進入 fatal；一般資源播放錯誤仍優先進入 fatal，不交由進化的非資源失敗策略處理。
- 控制器與其固定容量資料在 Game 建構時配置一次；沒有逐次進化配置。新增配置仍會占用 heap，不以靜態 RAM 數字推論 SRAM 峰值不變。

## 驗證

- `test/game_startup/run_host_test.ps1` 通過；沿用啟動、恢復、重置、無目標／同物種、立即進化、查詢失敗、首錯資源、真實 Pet 交易與 Status 回歸。
- 新增 16 個動畫進化案例：單段、雙段、重繪、來源缺失／入列拒絕、目標缺失、來源／目標逐幀失敗、契約重載失敗、存檔失敗、來源／目標資源 fatal，以及來源／目標期間的 reset 與 battery 中斷。驗證轉換時機、輸入阻擋、單次存檔與基礎動畫交接；Renderer／SD 使用 host fake。
- 同一份新增測試亦與重構前的 Game 標頭及兩份 cpp 編譯執行通過；既有正常與故障語意未改變。
- Animation Scene playback host 通過；Pet Behavior host 在 6／10 Stat 容量通過。
- Game、GameStartup、EvolutionController 於 Debug／Guess Game／Outfit／Predict／Appearance Selection／Startup／FirstStart／Sequential Status 旗標全開時通過 `g++ -fsyntax-only`；此組態未完整連結或執行。
- `platformio run -e project_29` 重構前後均成功：reported Flash 57,228 → 57,296 B（+68 B），reported 靜態 RAM 6,788 → 6,788 B。相同 STM32F103C8 設定及既有工具鏈；第三方 boolean deprecated 與 LTO serial compilation 警告保留。
- `platformio.ini` SHA256 前後一致：`E3B1653EEF5CBCB7F62BB8CD5A055FDE2B3D51FAB62803F896BC34AD26843B29`。
- 任務檔案空白檢查通過。未執行實機 SD／TFT／按鍵、GD32 build、設備時間或 SRAM 峰值量測。
