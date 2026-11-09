#if defined(PLATFORM_GD32_CLOCK_108MHZ) && PLATFORM_GD32_CLOCK_108MHZ

#include <Arduino.h>
#include "stm32f1xx_hal.h"

#if !defined(PLATFORM_MCU_GD32F103CBT6) || !PLATFORM_MCU_GD32F103CBT6 || \
    !defined(ARDUINO_GENERIC_F103CBTX)
#error "The 108 MHz clock is for the GD32F103CBT6 STM32 Core compatibility target."
#endif
#if defined(USBCON)
#error "The GD32 108 MHz clock does not configure the USB clock."
#endif
#if PREFETCH_ENABLE != 0
#error "Disable STM32 Flash prefetch for the GD32 108 MHz target."
#endif

namespace
{
static_assert(HSE_VALUE == 8000000UL, "The GD32 PLL requires an 8 MHz crystal.");

// GD32F10x RCU_CFG0 adds the high bits of PLLMF and ADCPSC to the F1 layout.
constexpr uint32_t kPllMultiplierHighBit = 1UL << 27;
constexpr uint32_t kAdcPrescalerHighBit = 1UL << 28;
constexpr uint32_t kPllMultiplyBy27 = kPllMultiplierHighBit | (10UL << RCC_CFGR_PLLMULL_Pos);
constexpr uint32_t kSystemClockHz = (HSE_VALUE / 2U) * 27U;
constexpr uint32_t kClockConfigMask =
    RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE |
    kAdcPrescalerHighBit | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
    RCC_CFGR_PLLMULL | kPllMultiplierHighBit;
// HXTAL / 2 × 27 gives 108 MHz. APB1 runs at 54 MHz and ADC at 13.5 MHz.
constexpr uint32_t kClockConfig = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV8 |
    RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | kPllMultiplyBy27;

bool waitFor(volatile uint32_t &reg, uint32_t mask, uint32_t expected)
{
    // The SysTick time base is suspended when restoring the clock after STOP.
    for (uint32_t attempts = 1000000; attempts != 0; --attempts)
    {
        if ((reg & mask) == expected)
            return true;
    }
    return false;
}

bool configureClock()
{
    RCC->CR |= RCC_CR_HSION;
    if (!waitFor(RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY))
        return false;
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_HSI);
    if (!waitFor(RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI))
        return false;

    RCC->CR &= ~RCC_CR_PLLON;
    RCC->CR |= RCC_CR_HSEON;
    if (!waitFor(RCC->CR, RCC_CR_PLLRDY, 0) || !waitFor(RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY))
        return false;

    MODIFY_REG(RCC->CFGR, kClockConfigMask, kClockConfig);
    if ((RCC->CFGR & kClockConfigMask) != kClockConfig)
        return false;

    RCC->CR |= RCC_CR_PLLON;
    if (!waitFor(RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY))
        return false;
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
    if (!waitFor(RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL))
        return false;

    // STM32 HAL's PLL decoder does not recognize GD32's multiplier high bit.
    SystemCoreClock = kSystemClockHz;
    return HAL_InitTick(HAL_GetTickPrio()) == HAL_OK;
}
}

// Overrides the generic STM32 Arduino variant at boot and on STOP recovery.
extern "C" void SystemClock_Config()
{
    // A pending button can prevent STOP. In that case the clock is still valid.
    if (SystemCoreClock == kSystemClockHz &&
        (RCC->CFGR & (kClockConfigMask | RCC_CFGR_SWS)) == (kClockConfig | RCC_CFGR_SWS_PLL))
        return;

    if (!configureClock())
        Error_Handler();
}

#endif
