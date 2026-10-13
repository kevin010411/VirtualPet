#include "audio/SequenceAudio.h"
#if ENABLE_SEQUENCE_AUDIO
#include <Arduino.h>
#include <SdFat.h>
#include <HardwareTimer.h>
#include "platform/BoardConfig.h"

namespace
{
constexpr uint16_t kHalfSamples = 512;
constexpr uint16_t kFadeSamples = 160; // 20ms at 8kHz.
SdBaseFile audioFile;
uint16_t samples[2 * kHalfSamples];
uint8_t readScratch[kHalfSamples];
volatile uint8_t refillMask = 0;
volatile uint32_t completed = 0;
volatile bool running = false;
volatile uint16_t fadeRemaining = 0;
uint16_t fadeCursor = 0;
uint32_t sampleCount = 0, loaded = 0;
volatile SequenceAudioError failure = SequenceAudioError::None;

void silenceHardware()
{
    TIM4->DIER = 0;
    TIM4->CR1 = 0;
    DMA1_Channel7->CCR &= ~DMA_CCR_EN;
    TIM2->CCR3 = 128;
    digitalWrite(BoardConfig::AudioAmpEnablePin, LOW);
    TIM2->CR1 = 0;
    running = false;
    fadeRemaining = 0;
}
bool fillHalf(uint8_t half)
{
    const uint32_t available = sampleCount - loaded;
    const uint16_t count = available < kHalfSamples ? available : kHalfSamples;
    if (count && audioFile.read(readScratch, count) != count) return false;
    for (uint16_t index = 0; index < kHalfSamples; ++index)
    {
        int16_t value = index < count ? int16_t(readScratch[index]) - 128 : 0;
        samples[half * kHalfSamples + index] = uint16_t(value + 128);
    }
    loaded += count;
    return true;
}
}

extern "C" void DMA1_Channel7_IRQHandler()
{
    const uint32_t flags = DMA1->ISR;
    DMA1->IFCR = DMA_IFCR_CGIF7;
    if (flags & DMA_ISR_TEIF7)
    {
        failure = SequenceAudioError::HardwareFailed;
        silenceHardware();
        return;
    }
    if (!running || fadeRemaining) return;
    // A stale half must never be replayed. Refill happens only in the main
    // loop; IRQ never accesses SD or shares its SPI bus with the display.
    for (uint8_t half = 0; half < 2; ++half)
    {
        const uint32_t flag = half == 0 ? DMA_ISR_HTIF7 : DMA_ISR_TCIF7;
        if (!(flags & flag)) continue;
        completed += kHalfSamples;
        if (completed >= sampleCount) { silenceHardware(); return; }
        const uint8_t next = uint8_t(1U << (1U - half));
        if ((refillMask & next) || (flags & DMA_ISR_HTIF7 && flags & DMA_ISR_TCIF7))
        {
            failure = SequenceAudioError::Underrun;
            silenceHardware();
            return;
        }
        refillMask |= uint8_t(1U << half);
    }
}

// TIM4 has no application owner; PA9 backlight remains on TIM1. During a
// requested short fade DMA stops and this sample-rate IRQ ramps to silence.
static void audioFadeTick()
{
    TIM4->SR = 0;
    if (fadeRemaining == 0) return;
    --fadeRemaining;
    const int32_t value = int32_t(samples[fadeCursor]) - 128;
    fadeCursor = (fadeCursor + 1) % (2 * kHalfSamples);
    TIM2->CCR3 = 128 + value * fadeRemaining / kFadeSamples;
    if (fadeRemaining == 0) silenceHardware();
}

namespace SequenceAudioPlatform
{
bool prepare(SdFat *sd, const SequenceAudioRef &reference)
{
    stop(false);
    failure = SequenceAudioError::None;
    sampleCount = reference.sampleCount;
    loaded = completed = 0;
    refillMask = 0;
    if (sd == nullptr || !sampleCount ||
        !audioFile.open("/audio.data", O_RDONLY) ||
        !audioFile.seekSet(reference.offset) || !fillHalf(0) || !fillHalf(1))
    {
        failure = SequenceAudioError::ReadFailed;
        audioFile.close();
        return false;
    }
    return true;
}
bool start()
{
    if (failure != SequenceAudioError::None || sampleCount == 0) return false;
    static HardwareTimer sampleTimer(TIM4);
    sampleTimer.pause();
    sampleTimer.setPrescaleFactor(72);
    sampleTimer.setOverflow(125, TICK_FORMAT);
    sampleTimer.attachInterrupt(audioFadeTick);
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
    pinMode(BoardConfig::AudioAmpEnablePin, OUTPUT);
    digitalWrite(BoardConfig::AudioAmpEnablePin, LOW);
    // F103 timer clock doubles PCLK1 when APB1 prescaler is not unity.
    uint32_t timerHz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0) timerHz *= 2;
    TIM2->CR1 = 0;
    TIM2->PSC = 0;
    TIM2->ARR = 255;
    TIM2->CCR3 = 128;
    TIM2->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
    TIM2->CCER = TIM_CCER_CC3E;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    TIM4->CR1 = 0;
    TIM4->PSC = timerHz / 1000000UL - 1;
    TIM4->ARR = 124;
    TIM4->CNT = 0;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->SR = 0;
    DMA1_Channel7->CCR = 0;
    DMA1->IFCR = DMA_IFCR_CGIF7;
    DMA1_Channel7->CPAR = reinterpret_cast<uint32_t>(&TIM2->CCR3);
    DMA1_Channel7->CMAR = reinterpret_cast<uint32_t>(samples);
    DMA1_Channel7->CNDTR = 2 * kHalfSamples;
    DMA1_Channel7->CCR = DMA_CCR_DIR | DMA_CCR_CIRC | DMA_CCR_MINC |
        DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_PL_1 |
        DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_TEIE | DMA_CCR_EN;
    NVIC_SetPriority(DMA1_Channel7_IRQn, 2);
    NVIC_EnableIRQ(DMA1_Channel7_IRQn);
    NVIC_SetPriority(TIM4_IRQn, 2);
    NVIC_EnableIRQ(TIM4_IRQn);
    running = true;
    digitalWrite(BoardConfig::AudioAmpEnablePin, HIGH);
    TIM4->DIER = TIM_DIER_UDE;
    TIM4->CR1 = TIM_CR1_CEN;
    return true;
}
void service()
{
    if (!running || fadeRemaining) return;
    for (uint8_t half = 0; half < 2; ++half)
    {
        const uint8_t bit = uint8_t(1U << half);
        if (!(refillMask & bit)) continue;
        if (!fillHalf(half))
        {
            failure = SequenceAudioError::ReadFailed;
            silenceHardware();
            return;
        }
        __disable_irq();
        refillMask &= ~bit;
        __enable_irq();
    }
}
uint32_t consumedSamples()
{
    __disable_irq();
    const uint32_t blocks = completed;
    const uint32_t pending = DMA1->ISR;
    const uint16_t remain = DMA1_Channel7->CNDTR;
    const bool playing = running && !fadeRemaining;
    __enable_irq();
    if (!playing) return blocks;
    uint32_t progress = 2 * kHalfSamples - remain;
    progress %= kHalfSamples;
    // A main-loop snapshot may precede the IRQ handler for a boundary.
    return blocks + progress + ((pending & (DMA_ISR_HTIF7 | DMA_ISR_TCIF7)) ? kHalfSamples : 0);
}
bool active() { return running; }
SequenceAudioError error() { return failure; }
void stop(bool fade)
{
    if (fade && running && !fadeRemaining)
    {
        DMA1_Channel7->CCR &= ~DMA_CCR_EN;
        fadeCursor = (2 * kHalfSamples - DMA1_Channel7->CNDTR) % (2 * kHalfSamples);
        fadeRemaining = kFadeSamples;
        TIM4->DIER = TIM_DIER_UIE;
    }
    else if (!fade)
    {
        // Registers cannot be touched before peripheral clocks are enabled.
        if (running) silenceHardware();
        pinMode(BoardConfig::AudioAmpEnablePin, OUTPUT);
        digitalWrite(BoardConfig::AudioAmpEnablePin, LOW);
    }
    audioFile.close();
}
}
#endif
