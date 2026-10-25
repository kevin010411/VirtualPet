# Runtime Table v26 升級驗證

2026-10-25，韌體目前工作樹已整合 v26。先前五項核心重構仍保留；本批尚未提交、推送或燒錄。
跨層資料契約以 Web `.scratch/sd-driven-pet-stats/spec.md` 為準，本文只記錄韌體變更與驗證。

## 整合範圍

來源為既有 `codex/instruction-response` 的 `b66af8b`，另帶入其工作樹中符合
2026-10-23 規格修訂的三個回饋動畫檔案。以共同祖先 `93fc8de` 三方合併，
保留主線簡化與未提交重構，沒有切換分支或建立 merge commit。

- Reader 僅接受 v26，支援 section 70／71 與 feature bit 15；保留舊格式拒絕行為。
- 整合指令反應命令、角色、選項繪圖、逐鍵時間戳、退出／低電壓取消及整場效果結算。
- 新資料解碼放入既有拆分後的 behavior decoder；裁切判定放入 presentation decoder。
- 新結算呼叫使用 `PetSaveResult::Failed` 判斷寫入失敗，保留一次消耗、失敗不重算。
- 回饋播放選中的動畫版本一次，包含最後一幀停留；內建勾／叉顯示後停留 500 ms。
  `feedback_ms` 保留於 wire record，產品播放流程不再使用手動值。
- 後續影格也在 SD／繪圖完成後才起算停留時間，避免慢速讀取吃掉最後一幀的顯示時間。

沒有新增 firmware 資料／素材驗證或掃描，也沒有更改存檔格式、正式 profile 或板子容量。
功能預設關閉，由匹配的 Web Profile Resolver 產物啟用。

## Host 證據

本次直接使用 `E:/C++/virtualPet/web/tests/fixtures` 的實際 v26 檔案，
沒有將 fixture header 改成 v22，也沒有沿用先前重構的合成副本作為相容證據。
`instruction_response_v23` 是歷史目錄名稱，其目前測試套件的 runtime header 為 v26。

通過項目：

- `runtime_table_behavior`：Death 0／1，共用 coherent、startup、sequence、optional 與錯誤案例。
  先前由 v26 fixture／v22 reader 引起的基準失敗已解除。
- `instruction_response`：完整作答期間的 easy／hard × normal／no-go、權重邊界、逐鍵去抖、
  期限邊界、退出仲裁、uint32 rollover 與完成只消耗一次。
- 真實 v26 指令反應套件：valid、minimum、weighted、effects、hard、nogo、maximum 皆通過
  reader／模型及 FrameDecoder；最大套件含 20 題、12 個 Stat、24 筆效果、八角色 × 四版本 × 兩幀。
- feature-off 接受 unused v26 套件、拒絕啟用套件；明確拒絕 v22／23／24／25／27 header。
- Game：指令反應單獨啟用，以及與 Guess／Predict／Collection／Death／音訊共存的 host。
  覆蓋 1／10／20 題、單幀／三幀回饋、50／10000 ms 影格間隔、首幀繪圖延遲、
  每幀 700 ms 讀取延遲、最後回饋故障／退出、低電壓取消、一次結算與寫入失敗不重算。
- Game 一般與 FirstStart／Appearance／Collection／Death、daily post 關閉、音訊 idle 生命週期。
- Animation Scene atomic first frame／完成／中斷／Idle；runtime contract loader、版面實際像素、
  Pet Behavior 6／10／12 Stat、FirmwareRandom、RTC／VoltageMonitor。

主要可重跑入口：

```powershell
pwsh -NoProfile -File test/runtime_table_behavior/run_host_test.ps1 -DeathEnabled 1
pwsh -NoProfile -File test/instruction_response/run_host_test.ps1 `
  -FixtureRoot E:/C++/virtualPet/web/tests/fixtures/instruction_response_v23/maximum `
  -FixtureKind maximum `
  -UnusedFixtureRoot E:/C++/virtualPet/web/tests/fixtures/instruction_response_v23/unused
pwsh -NoProfile -File test/game_startup/run_host_test.ps1 `
  -EnableInstructionResponse -EnableGuessGame -EnablePredict -EnableCollectionCatalog -DeathEnabled 1
```

執行時使用忽略目錄內的既有 host wrapper 分配獨立輸出路徑，避開舊 exe 的 Windows ACL。
該 wrapper 只改測試輸出位置，未替換資料或斷言。初次 Game 回饋測試按舊計時規則失敗，
已改為斷言新版可觀察播放規則並補上最後回饋退出／故障與慢速影格案例；最終組合通過。

## STM32F103C8 linked builds

固定相同 project_26、工具鏈、65536 bytes Flash／20480 bytes RAM 及其餘 flags：

| 組態 | Flash bytes | 靜態 RAM bytes | 剩餘 Flash bytes |
| --- | ---: | ---: | ---: |
| 正式 profile，指令反應關閉 | 61468 | 11624 | 4068 |
| 僅啟用指令反應 | 64808 | 12104 | 728 |
| 僅啟用音訊，指令反應關閉 | 65232 | 15340 | 304 |

指令反應 ON 與音訊對照使用 `.pio/v26-upgrade/on.ini`、`audio.ini`，
分別使用獨立 build 目錄。正式 `platformio.ini` 未修改；預設 build 目錄仍為正式 OFF 產物。
`arm-none-eabi-nm -C` 確認 OFF ELF 沒有 InstructionResponse／instructionInput／renderInstruction 符號。
日誌為 `.pio/v26-upgrade/build-off-final.log`、`build-on-final.log`、`build-audio.log`。

以上是 host／連結容量證據，靜態 RAM 不包含 heap／stack 峰值。
本次未修改 Web、未重新執行完整 Web 測試，也未驗收實體 SD／TFT／三鍵延遲／RTC／STOP／低電壓／斷電。
指令反應與音訊同時啟用有 host 證據，但本表不代表該組合已通過 C8 容量建置。
舊 SD 套件需要以匹配的 v26 exporter 重新匯出，runtime、manifest 與 assets 應使用同一份套件。
