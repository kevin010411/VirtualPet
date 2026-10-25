#ifndef PET_HAL_CONF_EXTRA_H
#define PET_HAL_CONF_EXTRA_H

// Override with -DENABLE_DEBUG_UART=1 to use the framework's default debug UART.
#ifndef ENABLE_DEBUG_UART
#define ENABLE_DEBUG_UART 0
#endif

#if ENABLE_DEBUG_UART != 0 && ENABLE_DEBUG_UART != 1
#error "ENABLE_DEBUG_UART must be 0 or 1."
#endif

#if !ENABLE_DEBUG_UART
#define DEBUG_UART NP
#endif

#endif // PET_HAL_CONF_EXTRA_H
