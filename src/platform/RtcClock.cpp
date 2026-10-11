#include "platform/RtcClock.h"
#include <Arduino.h>
#include "stm32f1xx.h"

// GD32F103 RTC/RCU/backup register offsets and bit fields match the F1
// CMSIS names used by the existing board framework. No RTC interrupt is enabled.
namespace
{
constexpr uint16_t marker = 0x5254;
bool waitSet(volatile uint32_t &reg, uint32_t mask, uint32_t timeout)
{
    const uint32_t start = millis();
    while ((reg & mask) != mask)
        if (static_cast<uint32_t>(millis() - start) >= timeout) return false;
    return true;
}
bool validSource()
{
    return (RCC->BDCR & (RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN | RCC_BDCR_LSEON |
                         RCC_BDCR_LSERDY | RCC_BDCR_LSEBYP)) ==
           (RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN | RCC_BDCR_LSERDY | RCC_BDCR_LSEON) &&
           BKP->DR1 == marker;
}
}

bool RtcClock::synchronize()
{
    if (!validSource()) return false;
    RTC->CRL &= ~RTC_CRL_RSF;
    return waitSet(RTC->CRL, RTC_CRL_RSF, 1000);
}

bool RtcClock::begin()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    if (BKP->DR1 == marker)
    {
        if (!synchronize()) return false;
        if ((RTC->PRLH & 0xfU) != 0 || (RTC->PRLL & 0xffffU) != 32767U ||
            !waitSet(RTC->CRL, RTC_CRL_RTOFF, 1000)) return false;
        RTC->CRH = 0;
        return waitSet(RTC->CRL, RTC_CRL_RTOFF, 1000);
    }

    RCC->BDCR |= RCC_BDCR_BDRST;
    RCC->BDCR &= ~RCC_BDCR_BDRST;
    RCC->BDCR |= RCC_BDCR_LSEON;
    if (!waitSet(RCC->BDCR, RCC_BDCR_LSERDY, 5000)) return false;
    RCC->BDCR |= RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN;
    RTC->CRL &= ~RTC_CRL_RSF;
    if (!waitSet(RTC->CRL, RTC_CRL_RSF, 1000) ||
        !waitSet(RTC->CRL, RTC_CRL_RTOFF, 1000)) return false;
    RTC->CRL |= RTC_CRL_CNF;
    RTC->CRH = 0;
    RTC->PRLH = 0;
    RTC->PRLL = 32767;
    RTC->CNTH = 0;
    RTC->CNTL = 0;
    RTC->CRL &= ~RTC_CRL_CNF;
    if (!waitSet(RTC->CRL, RTC_CRL_RTOFF, 1000)) return false;
    BKP->DR1 = marker;
    return true;
}

bool RtcClock::readSeconds(uint32_t &seconds)
{
    if (!validSource() || !(RTC->CRL & RTC_CRL_RSF)) return false;
    for (uint8_t attempt = 0; attempt < 4; ++attempt)
    {
        const uint32_t high = RTC->CNTH & 0xffffU;
        const uint32_t low = RTC->CNTL & 0xffffU;
        if (high == (RTC->CNTH & 0xffffU))
        {
            seconds = (high << 16) | low;
            return true;
        }
    }
    return false;
}
