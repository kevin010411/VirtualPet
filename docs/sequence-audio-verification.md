## 動畫配樂 v21（2026-10-15）

目前格式規格為 Web `contracts/runtime_table_v21.json` 與 `.scratch/sequence-audio/spec.md` 的 v21 節。下方 v20 行為／狀態歸屬僅為歷史記錄，已由動畫固定配樂取代。

韌體將 `ANIMATION_AUDIO` 的 logical ref 解析為既有 runtime animation ID，於啟用音訊時使用有界 lookup。AnimationController 對一般、系統與 base 動畫使用共同配樂入口；各輪及後續段重啟該動畫的音樂，段落結束／切換立刻停止，短音樂保留剩餘畫面的靜音。狀態切換但動畫相同時保留目前循環。沿用十輪 base 版本輪替及一次效果提交；資料／資源完整性驗證仍由 exporter 負責。

驗證：`test/sequence_audio/run_host_test.ps1 -FixtureRoot E:/C++/virtualPet/web/tests/fixtures/sequence_audio/animation_music -Animation` 通過，涵蓋真實 v21 匯出、同動畫跨情境、逐輪與後續段配樂、截止、短 EOF、一次效果、SD 失敗重試與取消。`-Disabled` host 亦通過。以上為 host 證據，未燒錄、未完成裝置音質／同步／電流驗收，也未聲稱正式韌體 Flash 連結容量通過。

---

# 播放序列音訊

產品／格式規格在 Web `.scratch/sequence-audio/spec.md` 與 `contracts/runtime_table_v20.json`。
Resolver 按實際使用音訊輸出 `ENABLE_SEQUENCE_AUDIO`，header 預設 `0`。
以下為韌體執行與 driver 限制。

`ENABLE_SEQUENCE_AUDIO` defaults to zero. The enabled build loads only bound
Action and state/default idle audio references and streams `/audio.data`; it adds no asset scans,
decode-only validation, or whole-file CRC checks. The exporter owns format,
duration, ownership, and reference validation.

`SequenceAudioPlayer` exposes preparation, start, monotonic playback time,
stop, and the first execution error. `AnimationController` prepares its audio
buffer before rendering the initial scene. The completed initial frame transfer
starts the sample clock. Visual frame selection follows consumed samples;
after EOF or failure it continues from the original monotonic start time.
Segment metadata is cached before playback. Repetitions and following segments
share one audio origin and never commit effects or tracked counts again.

Idle audio uses an independent base timeline. It follows the active State slot,
including the default slot, so two States sharing one animation can change music.
Each complete animation loop starts its audio from zero. A shorter clip stays
silent until the loop boundary; a longer clip stops there. Version rotation
retains one selection per ten complete animation loops. The base timeline never
occupies the Action queue or blocks daily processing, RTC catch-up, or idle sleep.
Actions, Status playback, menus, death and STOP stop it; returning to a living
base animation starts a new loop. A failed SD preparation retries only at a
complete loop, using the existing monotonic visual timeline in between.

Long audio continues buffered PCM with declining amplitude for 160 samples
(20 ms) when the visual sequence finishes. Accepted replacement, cancellation,
battery/fatal routing, and STOP use immediate stop. Finite playback blocks idle
sleep; the existing generic whole-sequence looping seam permits idle sleep and
restarts both tracks at the loop boundary. No infinite User Action authoring
option was added. Button beep writes cannot toggle the amplifier while audio
or its fade owns it. Wake redraw does not restore a sample position.

The STM32F103 target driver uses PA2 / TIM2 channel 3 for PWM, TIM4 update for
8 kHz pacing, DMA1 channel 7 for circular halfword transfers, and PA4 amplifier
enable. Two 512-sample halves use 2048 bytes, with a 512-byte SD read scratch.
SD refill happens in the main loop, never in IRQ. An unrefilled half stops
output and records underrun; SD and hardware failures likewise stop sound.
DMA stops before amplifier disable. The carrier is 281250 Hz at a 72 MHz timer
clock. PA9 backlight remains on TIM1. USART2 TX and I2C1 RX share DMA channel 7
and must remain unused; future TIM2/TIM4 features need explicit arbitration.
The code still targets `genericSTM32F103C8`. GD32 board identity, peripheral
equivalence, and actual timer clocks require device acceptance.

The source mapping was checked against the installed STM32duino F103 pin
table and [ST RM0008](https://www.st.com/resource/en/reference_manual/cd00171190.pdf).
TIM4 uses the framework's
[HardwareTimer interrupt dispatch](https://github.com/stm32duino/Arduino_Core_STM32/wiki/HardwareTimer-library),
avoiding a duplicate framework IRQ handler.

## 驗證入口與限制

`test/sequence_audio` host 使用真實 Web 匯出資料覆蓋長短音訊、重複／後續段、
替換／取消、一次效果提交、EOF、讀取失敗與 underrun；時間／TFT／PCM 輸出為 host 替代。
搭配 RTC／power 與 Game sequence-interruption hosts。歷史尺寸見 [Flash 評估](flash-feature-audit.md)。

`test/sequence_audio/run_host_test.ps1 -FixtureRoot <Web fixture> -Idle` 使用
`sequence_audio/idle_only`、`idle_reordered`、`shared_music` 的真實匯出資料，
覆蓋狀態身份、預設綁定、完整循環、短音訊 EOF、SD 失敗重試與中斷。
`-Disabled` 的 FixtureRoot 指向 `sequence_audio`，確認關閉播放器後仍讀取
無聲 v20，並拒絕 idle-only bit14。
`test/game_startup/run_host_test.ps1 -EnableSequenceAudio -IdleAudioOnly
-DeathEnabled 1 -EnableAppearanceSelection` 覆蓋 Game 日界、RTC 補時、
STOP 重繪、死亡與外觀選單的實際控制入口；硬體輸出仍為 host 替代。

實機尚待驗收 SD refill／TFT 負載、同步誤差、PWM／RC／擴大器／喇叭、
低電壓、電源峰值及 heap／stack。標準 C8 需以當次 resolver 設定重新連結確認容量。
