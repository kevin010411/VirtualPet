#include <Arduino.h>
#include "stm32f1xx_hal.h"

#if defined(PLATFORM_MCU_GD32F103CBT6) && PLATFORM_MCU_GD32F103CBT6 && !defined(STM32F103xB)
#error "GD32F103CBT6 requires the STM32F103 Arduino compatibility build."
#endif

// Retain the framework's init ordering and omit the unused serial event hook.
__attribute__((constructor(101))) static void firmwarePremain()
{
#ifdef NVIC_PRIORITYGROUP_4
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
#endif
#if (__CORTEX_M == 0x07U)
#ifndef I_CACHE_DISABLED
    SCB_EnableICache();
#endif
#ifndef D_CACHE_DISABLED
    SCB_EnableDCache();
#endif
#endif
    init();
}

void setup();
void loop();

int main(void)
{
    initVariant();
    setup();
    for (;;)
    {
#if defined(CORE_CALLBACK)
        CoreCallback();
#endif
        loop();
    }
}
