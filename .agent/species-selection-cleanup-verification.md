# 無入口物種選擇流程清理（2026-10-06）

- `startSpecies()` 的 graph inbound trace 無呼叫者；以當前 src/include/test 引用核對，沒有正式入口。
- 移除 AppearanceSelectionController 的物種選項、預設 Outfit、索引、模式、預覽、確認及專用選擇動畫；保留 Outfit 選擇、預覽、左右循環、鎖定與確認行為。
- 移除 Game::OnConfirmKey 的物種確認分支；保留 EvolutionHost::enterSpecies 與外觀交易。
- 沿專用列舉鏈移除 AppearanceLoader/SdAppearanceLoader::loadSpecies、loadRuntimeTableSpecies 及 decodeSpeciesQuery，以及 FakeAppearanceLoader 對應方法。
- 保留 Species 二進位段落、其必要段落檢查、Outfit range/preview/unlock/consumable 查詢及 Evolution 目標讀取。沒有二進位格式、角色或 Web exporter 變更。
- 保留 ENABLE_OUTFIT_CHOOSE_ANIMATION 定義及 RuntimeTableBehavior feature gate；Web backend/services/feature_sets/profile_resolver.py 的設定清單仍含此旗標。
- 移除列舉專用測試；snapshot 開關檔、manifest mismatch、截斷與重新讀取測試改用 validateRuntimeTableAppearance，adapter 首錯測試改用 loadOutfits。既有第二物種 Outfit 範圍、錯誤範圍、預覽與解鎖覆蓋保留。

驗證通過：

- `test/runtime_table_behavior/run_host_test.ps1`
- `test/runtime_contract_loader/run_host_test.ps1`
- `test/game_startup/run_host_test.ps1`
- `test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation`
- `platformio run -c .pio/action-refactor.ini -e project_29`：前後讀值 Flash 56,956 → 56,556 B，靜態 RAM 6,860 → 6,860 B。工作目錄原有其他重構修改；這是當前工作目錄前後建置讀值，沒有建立隔離的乾淨 commit A/B。
- `platformio run -c .pio/species-cleanup-outfit.ini -e project_29`：使用同一設定但啟用 ENABLE_COMMAND_OUTFIT，完整建置及連結通過；Flash 56,772 B，靜態 RAM 6,860 B。此臨時設定與 build 位於忽略的 .pio，沒有修改 platformio.ini。
- `git diff --check`；移除的物種選單／列舉符號在 src/include/test 無剩餘引用。

未執行實機 SD／TFT／按鍵驗收、完整 release gate、設備時間／heap 或 stack 峰值量測。未 stage、commit 或燒錄。
