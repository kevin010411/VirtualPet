# 外觀變更責任收斂（2026-10-06）

沿用工作區尚未提交的目錄整理與 EvolutionController；沒有 stage、commit、燒錄或修改 platformio.ini。修改前相關檔案保存在忽略目錄 `.pio/appearance-before/`。

## 責任

- AppearanceChangeController：`changeSpecies`、`applyOutfit`、`refreshUnlockState`。回報成功、契約設定失敗、Pet 拒絕、解鎖失敗或存檔失敗；保持已發生的狀態變更。遮罩刷新保留原 bool 結果與僅在遮罩變化時存檔的規則。
- Game：提供契約載入／啟用後的各模組設定；維持原 fatal、錯誤顯示、Debug 階段與選單退出決策。初次建立狀態使用已啟用契約，不重新讀 SD。
- AppearanceSelectionController 與 RuntimeContractLoader 保留原責任，未修改實作。
- EvolutionController 直接從 Pet snapshot 查詢目標，保留同物種 NoTarget 與 LoadFailed 行為。
- PetSaveController 保留兩 tick 存檔節奏、失敗不清除計數、成功清除計數及 Debug 訊息；移除 PetActionController。

## 順序與失敗

物種切換先啟用契約／Renderer，再依序設定 species、outfit、解鎖遮罩及存檔。無效 outfit 仍可能留下已切換的 species 與歸零的 stage_days／遮罩。解鎖或存檔失敗也不回復先前步驟。物種錯誤沿用 pet appearance、AppearanceLoader 首錯及 state_a/b.bin 資源名稱。

Outfit 先啟用契約；需要解鎖時才計算消耗 snapshot、提交扣值／解鎖／換裝，之後更新 Renderer 並存檔。存檔失敗保留已扣值、解鎖及換裝；契約已啟用但計算／提交失敗時，也不回復 Renderer 或契約。只有契約設定失敗仍觸發原 Game 錯誤顯示，沒有替後續失敗新增顯示。

## 驗證

- Game startup host 修改前與修改後皆通過，包含原啟動、恢復、重置、Evolution 正常／故障／中斷、fatal、真實 Pet 交易及 Status 案例。
- 新增 13 個直接外觀操作案例與遮罩／存檔節奏案例，覆蓋契約、Pet 部分暫存、解鎖、存檔故障，預啟用契約、一般換裝、消耗換裝及成功。以 host 事件序列及真實 Pet 狀態驗證執行順序、扣值保留、遮罩保留及存檔計數。
- Animation Scene playback、Pet Behavior（6／10 Stats）、Pet persistence host 通過。
- Game、GameStartup、EvolutionController、AppearanceChangeController、PetSaveController、AppearanceSelectionController 在 Debug／Guess Game／Outfit／Predict／Appearance Selection／Startup／FirstStart／Sequential Status 旗標全開時通過 `g++ -fsyntax-only`；全開組態未完整連結或執行。
- 同一 `platformio run -e project_29` 設定下，重構前 Flash 57,296 B／靜態 RAM 6,788 B；重構後 Flash 57,372 B（+76 B）／靜態 RAM 6,828 B（+40 B）。新控制器直接由 Game 持有，移除原 PetActionController 的一次 heap 配置；靜態 RAM 增量不能代表 SRAM 峰值變化。
- 任務檔案空白檢查通過；platformio.ini SHA256 維持 `E3B1653EEF5CBCB7F62BB8CD5A055FDE2B3D51FAB62803F896BC34AD26843B29`。
- 未執行實機 SD／TFT／按鍵、GD32 build、設備時間或 SRAM 峰值量測。第三方 boolean deprecated 與 LTO serial compilation 警告保留。
