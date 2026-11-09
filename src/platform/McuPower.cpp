#include "platform/McuPower.h"
#include "platform/VoltageMonitor.h"

#include <Arduino.h>
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

namespace
{
// The STM32F103 and GD32F103 compatibility builds use the same detector bits.
constexpr unsigned long kPvdSettleUs = 50;
constexpr uint8_t kPvdLevelDimmed = 7;
constexpr uint8_t kPvdLevelCritical = 6;

void setPvdThreshold(uint8_t level)
{
    PWR->CR = (PWR->CR & ~(7U << 5)) | ((static_cast<uint32_t>(level) & 7U) << 5);
}

bool isVddBelowPvdThreshold(uint8_t level)
{
    // Restart the comparator to isolate hysteresis when sampling two levels.
    PWR->CR &= ~PWR_CR_PVDE;
    setPvdThreshold(level);
    PWR->CR |= PWR_CR_PVDE;
    delayMicroseconds(kPvdSettleUs);
    return (PWR->CSR & PWR_CSR_PVDO) != 0;
}
}

void McuPower::beginVoltageDetector()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    setPvdThreshold(kPvdLevelDimmed);
    PWR->CR |= PWR_CR_PVDE;
}

VoltageState McuPower::sampleVoltage()
{
    const bool belowDimmed = isVddBelowPvdThreshold(kPvdLevelDimmed);
    const bool belowCritical = isVddBelowPvdThreshold(kPvdLevelCritical);
    setPvdThreshold(kPvdLevelDimmed);

    if (belowCritical)
        return VoltageState::Critical;
    if (belowDimmed)
        return VoltageState::Dimmed;
    return VoltageState::Normal;
}

void McuPower::sleepIfIdle(bool (*shouldSleep)(uint32_t), uint32_t wakeGeneration)
{
    PWR->CR &= ~PWR_CR_PVDE;
    HAL_SuspendTick();
    // A pending EXTI wakes WFI even with PRIMASK set. Preserve a late edge.
    __disable_irq();
    if (shouldSleep(wakeGeneration))
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    __enable_irq();
    SystemClock_Config();
    HAL_ResumeTick();
    PWR->CR |= PWR_CR_PVDE;
}
