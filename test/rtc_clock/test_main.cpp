#include <cassert>
#include <cstdio>
#include "stm32f1xx.h"
#include "platform/RtcClock.h"

RccRegisters rcc;
PwrRegisters pwr;
BackupRegisters backup;
RtcRegisters rtc;
static unsigned long ticks = 0;
static bool oscillator = true, synchronization = true;
unsigned long millis()
{
    ++ticks;
    if (oscillator && (rcc.BDCR & RCC_BDCR_LSEON)) rcc.BDCR |= RCC_BDCR_LSERDY;
    if (synchronization && (rcc.BDCR & RCC_BDCR_RTCEN))
        rtc.CRL |= RTC_CRL_RSF | RTC_CRL_RTOFF;
    return ticks;
}

int main()
{
    uint32_t seconds = 99;
    assert(!RtcClock::readSeconds(seconds) && seconds == 99);
    assert(RtcClock::begin());
    assert(rtc.PRLH == 0 && rtc.PRLL == 32767 && rtc.CRH == 0);
    assert(!(rtc.CRL & RTC_CRL_CNF));
    rtc.CNTH = 0x1234;
    rtc.CNTL = 0xabcd;
    assert(RtcClock::readSeconds(seconds) && seconds == 0x1234abcd);
    rtc.CRH = 7;
    assert(RtcClock::begin());
    assert(RtcClock::readSeconds(seconds) && seconds == 0x1234abcd && rtc.CRH == 0);
    rtc.PRLL = 123;
    assert(!RtcClock::begin()); // Never accept a retained wrong divider.
    rtc.PRLL = 32767;
    rcc.BDCR |= RCC_BDCR_LSEBYP;
    assert(!RtcClock::readSeconds(seconds));
    rcc.BDCR &= ~RCC_BDCR_LSEBYP;
    synchronization = false;
    const unsigned long beforeSync = ticks;
    assert(!RtcClock::synchronize() && ticks - beforeSync <= 1002);
    synchronization = true;
    assert(RtcClock::synchronize()); // Wake restores register synchronization only.
    assert(RtcClock::readSeconds(seconds) && seconds == 0x1234abcd);
    backup.DR1 = 0;
    rcc.BDCR = 0;
    oscillator = false;
    const unsigned long beforeStart = ticks;
    assert(!RtcClock::begin() && ticks - beforeStart <= 5002);
    assert(backup.DR1 == 0);
    puts("[PASS] Simulated RTC initialization, retained counter, source/divider rejection and bounded failures");
}
