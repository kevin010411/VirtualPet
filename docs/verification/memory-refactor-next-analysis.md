# 下一輪 Flash／RAM 精簡分析（2026-10-31）

本輪只分析，沒有修改正式程式或燒錄。延續使用者限制：功能、224 Outcomes／2,688 effects、256 Versions 保留；不縮減繪圖／讀取／音訊緩衝，不減 12 行繪圖批次，不把常駐播放資料改成 SD 即時查詢。

## 證據範圍

沿用第一輪 compact 的固定配置：maximum 60,592 B Flash／9,960 B static RAM；numeric200 62,972／13,136；allfeatures rejected map 75,504／14,964，仍超出 Flash 9,968 B。這輪沒有新 firmware build，不能宣稱下列方案已省下容量。

分析目前來源、compact/numeric.elf 的 ARM nm 符號與 allfeatures.map 的依賴。另以相同 Cortex-M3 ABI 編譯 `.pio/memory-refactor/next-audit/types.cpp`，只量測型別及候選資料形狀，沒有連結入正式韌體。候選 RAM 數字是欄位／陣列推算；最終須再比較同配置 map 與 binary。

## 1. Flash 優先：固定硬體避免帶入通用 PWM／Timer 路徑

`src/platform/main.cpp::writeBacklightPwm` 對固定 PA9 使用 analogWrite；硬體已固定 TIM1。`src/audio/SequenceAudioPlatform.cpp::start` 大部分已直接設 TIM2／TIM4／DMA，仍建立 HardwareTimer 來註冊 fade callback。可改為固定 TIM1 背光驅動及有明確中斷所有權的 TIM4 fade handler，保留亮度、漸亮、音訊 fade 與 STOP 恢復。

numeric ELF 中 analogWrite 本體 1,060 B、HardwareTimer constructor 384 B；這不是可直接相加的預期節省。allfeatures 中音訊也引用 HardwareTimer，單改背光不會移除所有相關依賴。必須同時審查框架 IRQ ownership，不可直接新增與框架衝突的 TIM4_IRQHandler。

這條路徑不修改 TFT SPI／DMA／FrameDecoder，預期不降低繪圖吞吐，但音訊時序與低電壓／睡眠仍需硬體驗證。

## 2. Flash 優先：消除無用途的標準輸出／UART 依賴

allfeatures.map 明確顯示 HardwareTimer → std::__throw_bad_function_call → abort，以及 exit → stdio cleanup。Print.cpp 也帶入 vdprintf／格式化家族，是另一個入口，不能假設只移除 Timer 即可全部消失。

numeric ELF 仍有 `_write` 912 B、HAL_UART_IRQHandler 976 B、四張 UART pin maps 合計 240 B，另有 UART 配置／回呼；`__sf` 占 static RAM 312 B。這些是目前存在的符號大小，並非已證明可回收的總量。

應先追清實際活躍呼叫與框架 weak hooks，再用專案內的精簡平台整合排除未使用的 serial／stdio。保留 debug build 的診斷能力，避免修改全域工具鏈快取；不可把真實錯誤處理換成吞錯。重編後確認這些符號是否真正消失。

## 3. RAM 優先：播放資料 8 → 6 B

`include/pet/PetBehaviorActionTypes.h` 的 PetBehaviorAnimationPlaybackConfig 目前是 AnimationRef 4 B、playbackCount 1 B、versionSelection 2 B，因對齊成 8 B。現行 versionSelection 為 0=隨機、1..256=固定版本，不能直接轉 uint8。

候選表示：AnimationRef 4 B、播放次數及 fixed flag 合併 1 B、實際 versionIndex 1 B。播放次數合法上限為 5，使用一個明確的 fixed flag 仍保留全部 256 Versions 與隨機選擇。應集中 accessor，避免散落位元運算；不使用 packed／未對齊存取，不加入 SD 查詢。

ARM 探針確認候選 Playback 6 B、Outcome 10 B（目前 12）、ActionCondition 32 B（目前 40）。按最大固定陣列推算：Outcomes 448 B、Conditions 256 B、FollowingAnimations 48 B、Actions 約 16 B，共約 768 B。最後 Config 對齊及 Flash 成本仍要正式連結確認。

另一種簡化是讓 exporter 依 priority 穩定排序條件，再刪掉 condition.priority；它也可能讓 Condition 40 → 32 B，但與前項 Condition 的 256 B 收益重疊，不能重複計算。需要確認相同優先權的原順序與全部呼叫者。

## 4. RAM：ScreenRule 使用分離陣列，消除尾端對齊

`ScreenRuleConfig` 的 4+4+2 B 目前對齊成 12 B。保存成獨立 uint32 minimum[]／maximum[] 與 uint16 frame[] 可降至每筆 10 B，保留兩個完整界限、同樣容量及 O(1) 索引，無需 SD 存取。

numeric200 的陣列預算可少 400 B；最大 1,600 rules 可少 3,200 B。allfeatures 比較配置只有一筆 rule，因此此項不解決該配置的主要缺口。不建議 packed struct，也不在本輪假設可刪 maximum：必須先證明所有有效匯出的分段都允許由下一筆推導。

## 5. RAM：signed64 effects 的 metadata 與 value 分開

ARM 探針確認 ActionEffect、GuessEffect、InstructionEffect 目前皆 16 B，主要是 8 B value 的對齊。value 不能直接降成 int32：目前既有功能保留 signed64 Change／Set 與每日變化。

改為自然對齊的 int64 values[] 加上小型 metadata 陣列：ActionEffect metadata 2 B，12 筆約少 72 B；Guess 保留現有 4 B metadata，48 筆約少 192 B；Instruction metadata 3 B，24 筆約少 120 B。全開配置合計原始陣列預算約少 384 B，最終外層對齊另計。

保留既有整組效果先讀取／試算成功後一次提交的原子性；不與正在使用的 audio／renderer buffer 共用記憶體。

## 6. Flash：仍有 loader 與執行期的重複語意驗證

`RuntimeTableBehaviorDecoder.cpp` 的 PetState／ActionCondition 還在檢查 reserved、ordinal、isRuntimeBehaviorRange、語意上的 playbackCount；Buttons 還有 reserved 與 ID 排序。`StatusSetContract.cpp` 在每次解析仍重新計算 levels 乘積、比較 versionCount、重驗 PetState mask 的選取數。

這些適合由 exporter 保證，韌體只保留實際將索引的陣列界限、容量、實際 SD 讀取及必要執行前置條件。`loadCompleteRuntimeTable` 與 `decodeRuntimeTableBehavior` 目前亦各清零一次同一 Config；可統一清零責任。

刪除前需逐條對照 backend 已有的正式驗證與測試，不能把所有 false 分支一律移除，尤其是讀取失敗、未編入功能、除零／索引安全。這項 Flash 收益尚未實測，預期不能單靠它補足 9,968 B。

## 7. 次優先：通用文字渲染及常駐物件所有權

numeric ELF 的 Adafruit 內建 font 1,280 B、drawChar 688 B、GFX::write 258 B。正式模式仍須顯示 SD／素材錯誤，不能因 debug=0 就刪字型。可評估保留同樣錯誤資訊的小型文字 renderer 或專用 TFT adapter；單一 print override 未必讓通用 GFX virtual methods 被 linker 回收，故先做獨立連結驗證再決定，不改像素串流熱路徑。

Game constructor 透過多個 new 建立幾乎終身存在的 Controller。可評估由值成員直接持有，減少配置與間接層，但移入 `.bss` 只是 heap→static 搬移，不等於省下物件容量。SdFat／框架也可能仍需要 allocator，不能宣稱能刪 malloc。應先完整列出 heap 物件及峰值，再判斷收益。

RuntimeRangePredicate 現為 24 B，但條件上下界仍是 signed64 契約，可能有負值或超出 uint32 的界限；不可直接照上一輪非負顯示規則的做法截低 32 位。此項需要先定義等價的邊界正規化與空集合表示，排在後面。

## 建議執行次序與每階段驗證

1. 固定 PWM／Timer 與 serial／stdio 依賴：優先處理 Flash，逐個替換後用同三配置量測；保留音訊、背光漸亮及 sleep/wake。
2. 播放表示與 ScreenRule 對齊：無額外 I/O 的 RAM 壓縮；驗證隨機／固定 256 邊界、224 結果、8 Outfit、逐像素及同值不重畫。
3. effects 陣列及剩餘重驗：維持效果原子性、signed64 極值、短讀與錯誤路由；重新比較 Flash/static RAM。
4. 字型／物件生命週期視前面實測再決定。同步量測所有 heap、stack high-water 與真正裝置播放時間，避免 static RAM 下降掩蓋峰值風險。

保留 bundle 身分比對、實際短讀處理、FrameDecoder 輸出界限、存檔 A/B＋CRC、繪圖／音訊雙緩衝與快取。存檔由裝置寫入，CRC／原子性無法交給 Web backend；這些不是重複匯出驗證。
