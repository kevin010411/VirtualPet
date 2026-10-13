# 播放序列音訊

產品／格式規格在 Web `.scratch/sequence-audio/spec.md` 與 `contracts/runtime_table_v19.json`。
Resolver 按實際使用音訊輸出 `ENABLE_SEQUENCE_AUDIO`，header 預設 `0`。
以下為韌體執行與 driver 限制。

`ENABLE_SEQUENCE_AUDIO` defaults to zero. The enabled build loads only bound
Action audio references and streams `/audio.data`; it adds no asset scans,
decode-only validation, or whole-file CRC checks. The exporter owns format,
duration, ownership, and reference validation.

`SequenceAudioPlayer` exposes preparation, start, monotonic playback time,
stop, and the first execution error. `AnimationController` prepares its audio
buffer before rendering the initial scene. The completed initial frame transfer
starts the sample clock. Visual frame selection follows consumed samples;
after EOF or failure it continues from the original monotonic start time.
Segment metadata is cached before playback. Repetitions and following segments
share one audio origin and never commit effects or tracked counts again.

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

實機尚待驗收 SD refill／TFT 負載、同步誤差、PWM／RC／擴大器／喇叭、
低電壓、電源峰值及 heap／stack。標準 C8 需以當次 resolver 設定重新連結確認容量。
