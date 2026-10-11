#pragma once
#include <stdint.h>
struct RccRegisters { volatile uint32_t APB1ENR = 0, BDCR = 0; };
struct PwrRegisters { volatile uint32_t CR = 0; };
struct BackupRegisters { volatile uint32_t DR1 = 0; };
struct RtcRegisters
{
    volatile uint32_t CRH = 0, CRL = 0, PRLH = 0, PRLL = 0, CNTH = 0, CNTL = 0;
};
extern RccRegisters rcc;
extern PwrRegisters pwr;
extern BackupRegisters backup;
extern RtcRegisters rtc;
#define RCC (&rcc)
#define PWR (&pwr)
#define BKP (&backup)
#define RTC (&rtc)
#define RCC_APB1ENR_PWREN (1U << 28)
#define RCC_APB1ENR_BKPEN (1U << 27)
#define PWR_CR_DBP (1U << 8)
#define RCC_BDCR_LSEON (1U << 0)
#define RCC_BDCR_LSERDY (1U << 1)
#define RCC_BDCR_LSEBYP (1U << 2)
#define RCC_BDCR_RTCSEL (3U << 8)
#define RCC_BDCR_RTCSEL_0 (1U << 8)
#define RCC_BDCR_RTCEN (1U << 15)
#define RCC_BDCR_BDRST (1U << 16)
#define RTC_CRL_RSF (1U << 3)
#define RTC_CRL_CNF (1U << 4)
#define RTC_CRL_RTOFF (1U << 5)
