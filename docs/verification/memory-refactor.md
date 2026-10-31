# 韌體記憶體重構驗證（2026-10-31）

最新已完成 [後續架構重構與容量比較](next-architecture-refactors.md)：全功能 Flash 74,396 B，仍缺 8,860 B；靜態 RAM 16,092 B 加固定 owned heap 3,908 B 為 20,000 B，未含 allocator／其他庫／stack／ISR。context／loader／Instruction 呈現及播放互斥狀態已整理；Game 直接成員實驗因全功能 Flash 回退撤回。額外 Web Instruction 分區仍有兩項權重格式契約失敗，詳見報告。

前輪 [Renderer 資源拆分](renderer-resource-split.md)：全功能 Flash 74,528 B，靜態 RAM 16,092 B 加固定 owned heap 3,912 B 為 20,004 B。read buffer 移入常駐資源 owner，不能只比較靜態 RAM 判斷收益。下方繼續保留歷史量測。

後續已完成 Game 結算／CommandResult 與前景狀態兩階段重構：[實作、量測與驗證報告](game-foreground-refactor.md)。目前最終 maximum 59,832／9,960 B、numeric200 62,184／13,136 B、allfeatures 74,640／14,964 B（Flash／靜態 RAM）；全功能仍超出 Flash 9,104 B。以下保留前輪各階段歷史數字。

本輪依使用者要求移除重複驗證、精簡常駐資料，保留現有功能與最大抽籤容量。使用者同時指定播放速度尚未達標，因此取消原先縮小顯示緩衝的方案，也沒有把結果／顯示規則改成播放時按需讀 SD。

來源基準是 `2b47d2c40d1f85547c9ecbc9fdc55e8bc867e407` 加上當時既有工作區修改。未提交、推送、部署或燒錄。既有 platformio.ini、水平像素幾何／素材尺寸改動及 custom_layout_export 測試修改沒有回退。

## 變更

第一階段移除 SCREEN_BLOCKS 的幾何、重疊、產品影格重算，移除 Status loader 的 mask／levels／Version 乘積重驗、素材 reserved=0 檢查及重複欄位清零。移除 `Renderer::validateLayoutVersion` 及其唯一使用者，在版面切換時少做兩次產品 metadata 查詢；真正讀取或繪圖失敗仍走既有資源錯誤處理。

第二階段將 Outcome 欄位重排為 12 B（原 14 B），將有效匯出資料中的非負 ScreenRule 上下界保存為 uint32，使每筆規則 24→12 B。Runtime v27 的 wire 上下界仍為 i64，讀取已驗證的低 32 位；Pet Stat 的 0..4294967295 與 signed64 效果／日變化不變。Section 不再保存只在讀目錄時使用的 length，RuntimeTable 由 1,064→808 B；manifest 讀取不再建立／重讀整份 section 目錄。

沒有增加動畫播放或數值更新的 SD 查詢。保持讀取緩衝 1,024 B、行緩衝 128×12×2=3,072 B、batch=12，以及現有 TFT 串流寫入、codec 和選擇性重畫策略。

## 固定配置的實際連結結果

以 `test/lottery_capacity/measure_refactor.ps1` 量測。三階段使用同一工具鏈、庫路徑與各配置完全相同的功能旗標；不覆寫使用者的 platformio.ini。目標為 genericSTM32F103C8，64 KiB Flash／20 KiB RAM，`-Os -flto`，debug 關閉。

所有配置均保留 12 Stats、224 Outcomes、12 筆本地 effects 儲存（總效果表仍支援 2,688 筆）。maximum 使用一筆 ScreenRule 及 startup；numeric 使用 200 筆 ScreenRule、Guess、startup；allfeatures 使用一筆 ScreenRule，另開音訊、Guess、Predict、手動換裝／外觀選擇、圖鑑、指令反應、Death、FirstStart、OutfitChoose 與 Guess 子模式。

| 配置 | 基線 Flash / 靜態 RAM | 移除重驗後 | 壓縮資料後 | 累計節省 Flash / RAM |
| --- | ---: | ---: | ---: | ---: |
| maximum | 62,024 / 10,432 B | 60,684 / 10,424 B | 60,592 / 9,960 B | 1,432 / 472 B |
| numeric（200 rules） | 64,336 / 16,000 B | 63,012 / 15,992 B | 62,972 / 13,136 B | 1,364 / 2,864 B |
| allfeatures | 76,888 / 15,428 B | 75,580 / 15,428 B | 75,504 / 14,964 B | 1,384 / 464 B |

maximum 與 numeric 在三階段皆完成連結。allfeatures 三階段均超過 64 KiB，最終仍超出 9,968 B，不是可燒錄驗收結果。

Flash 是 map 中實際載入 Flash 的 sections（含向量表及初始化資料）；成功配置已比對 firmware.bin 長度完全一致。PlatformIO 的摘要可能排除向量／初始化 sections，不能將其數字混入此表。失敗配置的數字取自 rejected map，不代表產生了成功 image。這是本輪新建的固定基線，與上一輪使用不同配置的 60,404 B 不混算。

## RAM 的範圍

表格 RAM 僅為 `.data + .bss`。Renderer 的 `new AnimationState` 另需 4,216 B（其中緩衝 4,096 B），三階段均未縮減。ARM 型別探針使用目前正式 headers 與 Renderer 定義、Cortex-M3/Thumb ABI；硬體 dependency stubs 只供型別編譯，未連結入正式韌體。

最終 maximum 的 static＋此 Renderer heap 為 14,176 B；numeric 為 17,352 B；allfeatures 的 rejected map 加此 heap 為 19,180 B。這些數字還沒有包含 allocator 管理、其他庫可能使用的 heap、呼叫堆疊與中斷用量，不能當作峰值 RAM。

解析用 RuntimeTable 減少 256 B 區域儲存，manifest 不再有 64×16=1,024 B 的來源級局部目錄。最終機器碼的最大堆疊與實機可用餘裕尚未量測。

## 驗證

- Web `scripts/preflight.ps1 -BackendOnly` 通過。
- Web `scripts/verify.ps1 -TestGroup screen-layout -BackendOnly -Completion`：17 tests、architecture 通過。幾何、唯一動畫、範圍／素材與輸出契約的主要驗證保留在後端；韌體測試調整為信任語意，仍拒絕容量、實際使用的規則範圍及缺少必要 sections。這不是 Web 全量驗證。
- `test/pet_behavior_runtime/run_host_test.ps1`：Stats 6/10/12、最大 224 Outcomes、讀取失敗不部分提交、固定 Version／重播、Catalog OFF/ON 皆通過。
- `test/runtime_table_behavior/run_host_test.ps1`：Death OFF/ON、v27、legacy rejection、appearance／序列／可選動畫及容量界限通過。
- 同 reader 的 `-LotteryFixture E:/C++/virtualPet/web/.verification/lottery-evidence/maximum-bundle/runtime.bin`：實際 Web 匯出的 8 Outfit × 224 結果映射與 2,688 effects 通過。
- `test/custom_layout_export/run_host_test.ps1` 及 `-Numeric`：完整真實輸出 frame 的逐像素比對、自訂水平幾何、200 分段端點、只更新數值矩形、不重畫相同值／相同圖通過。
- `test/animation_scene_playback/run_host_test.ps1` 及 `-AppearanceRedrawOnly`：首幀前版面同步、結束／中斷、還原畫面與 UINT32_MAX−1／UINT32_MAX 分段切換通過。版面切換前少兩次 metadata 查詢，TFT 圖像寫入序列維持。
- bundle_reader_on_access、runtime_contract_loader 通過；短讀、錯誤 payload／palette、實際 descriptor 失敗與錯誤資源路由保留。
- game_startup 的 StatusOnly、EvolutionOnly、DeathOnly 在 startup／appearance／catalog／audio／death 開啟配置通過。
- 擴大為 InstructionResponse＋Predict 等全功能 host 組合時，Instruction Response 部分通過，但 Predict reset 分支失敗於 `!game.hasTransientAnimation()`。相同旗標在重構前來源快照重現同一斷言（基線 line 2420，重構後 line 2418，差異是刪去兩行 mock 方法），故未將整套標記通過，也未藉此變更遊戲行為。
- `git diff --check` 通過；量測腳本語法通過，最終實際 binary 長度與 map Flash 一致。

未做實機 FPS、SD/TFT 時序、峰值 stack/heap 或燒錄驗收。本輪證據能確認緩衝／批次保留、未新增繪圖讀取、輸出像素與局部更新正確，不能宣稱已達到使用者的播放速度硬指標。

## 重跑與證據

```powershell
.\test\lottery_capacity\measure_refactor.ps1 -Stage <label>
```

腳本只編譯當下來源，不會切換／還原程式。要比較歷史階段，應使用相應來源快照，不可只換 label。輸出位於 `.pio/memory-refactor/<label>/`，包含固定 ini、三組完整 logs、map、sizes.json、成功配置 ELF，以及 ARM 型別探針與 types.json。PlatformIO 需要已安裝工具鏈的快取寫入權限。

本輪 baseline／validation／compact 的 map 與 sizes.json 已保存；baseline 的初始使用者 diff 在 `initial/user-before.patch`，validation／compact 的來源 diff 在各自 source.patch。host logs 位於 `.pio/memory-refactor/` 的 `final-*.log`，失敗前後對照在 `baseline/game-allfeatures.log` 及 `final-game.log`。

全功能 Flash 缺口與峰值 RAM／實機速度仍待下一階段處理。後續應先依相同配置的 map 找出真正的大型程式段；繼續遵守不降低功能／容量、不中斷既有素材身分比對、不犧牲繪圖速度的要求。

## 第三階段：剩餘語意重驗移除（semantic）

使用者於同日明示移除剩餘重複語意驗證，再檢查架構。此次正式程式只做驗證責任收斂，架構建議另記於 [firmware-architecture-audit.md](E:/C++/virtualPet/main/code/docs/verification/firmware-architecture-audit.md)，未依建議重寫 Controller 或繪圖流程。

移除 PetState ordinal／reserved／領域重驗、Action mode 對結果數的語意重驗及子表完整覆蓋比對、ActionCondition reserved／playback／領域重驗、Buttons ordinal／reserved／空綁定值重驗、Status ordinal／子表 ownership 重驗、Evolution 播放模式／次數配對與 reserved 重驗，以及 runtime header reserved 與 asset ordinal／shared-scope 一致性重驗。Evolution／Unlock 的 predicate 改成直接解碼，matchesRuntimeRange 不再先做一次形狀驗證；實際取值及 inclusive 比較仍在。

Status 執行時不再計算 levels 乘積並比對版本數，直接使用 exporter 算好的 versionCount；PetState Other 直接採 levels−1，只有被選中的狀態才計算前面選取 bit 的 rank。保留零 levels、活動 slot、condition 容量、實際 versionIndex 的 256 界限與 callback 讀值失敗處理。Behavior／Presentation decoder 不再重複清零已由完整載入入口初始化的 config；兩個正式呼叫者均已確認初始化。

後端正式 owner 為 `backend/infrastructure/runtime_table/format.py`：PetState ordinal／reserved／range、Action ranges／mode／playback／reserved、Buttons 綁定、Status mask／levels／Version product、Evolution mode／playback／reference 及 runtime reserved 等檢查仍保留。韌體持續檢查實際容量／使用範圍、必要 sections、record size／實體檔案界限、未編入的功能、bundle 身分、SD／codec／硬體執行失敗。並未移除存檔 CRC 或錯誤資源呈現。

三組配置、工具鏈及計算方法與 compact 完全相同；成功 binary 長度亦與 map Flash 計數一致。中途先移除 PetState／Action／Status 所得的 59,988／74,968／62,332 B 是非最終結果；下表才是補齊 Evolution／runtime 重驗後的最終數字。

| 配置 | compact Flash → semantic Flash | 本階段省 Flash | 靜態 RAM（前後相同） |
| --- | ---: | ---: | ---: |
| maximum | 60,592 → 59,688 B | 904 B | 9,960 B |
| numeric200 | 62,972 → 62,036 B | 936 B | 13,136 B |
| allfeatures | 75,504 → 74,648 B | 856 B | 14,964 B |

allfeatures 仍超出 64 KiB Flash **9,112 B**，為 rejected map，不能燒錄驗收。Renderer heap 維持 4,216 B，內含 4,096 B 顯示緩衝；沒有減少 batch、快取，或增加播放熱路徑 SD 查詢。表格不含其他 heap 與 stack，仍未測實機 FPS／峰值 RAM。

此階段的來源、logs、maps、sizes.json、types.json、成功 ELF 與 source-hashes.json 位於 `.pio/memory-refactor/semantic/`；source.patch 是相對原 HEAD 的工作區快照，包含前兩階段與既有使用者修改，不是僅本階段差異。

驗證證據：Web preflight 通過；canonical `behavior -BackendOnly -Completion` 29 tests、`evolution -BackendOnly -Completion` 53 tests 及各自 architecture checks 通過。兩者是 scoped verification，未宣稱全套 Web 通過。Firmware host 的 Action 原子效果／Stats 6/10/12／224 結果／Catalog OFF/ON、Status 含 256 Versions 與執行界限、RuntimeValueResolver、Game Status／Evolution、Animation Scene 與 RuntimeContractLoader 通過。最終來源的 RuntimeTable reader 在 Death OFF／ON 均通過，實際 Web 最大匯出 8 Outfit × 224 Outcomes／2,688 effects 全部映射亦通過。

舊 host 中「錯誤 child ownership 必須由 firmware 拒絕」及「Evolution 次數不合法必須由 firmware 拒絕」斷言調整為信任 exporter；backend 格式驗證仍負責拒絕，韌體容量／短讀／實際資源失敗與功能相容性案例保留。完整有效行為與最大實際 Web 匯出映射必須繼續通過，不能用改斷言代替。

上一輪已在重構前重現的 Predict reset 失敗未於本輪修復，也沒有將全功能 host 或裝置驗收標為通過。未提交、推送或燒錄。
