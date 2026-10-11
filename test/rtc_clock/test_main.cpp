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
    rtc.CRL &= ~RTC_CRL_RSF; // APB shadow is not yet synchronized after STOP.
    assert(!RtcClock::readSeconds(seconds));
    assert(RtcClock::synchronize());
    assert(RtcClock::readSeconds(seconds) && seconds == 0x1234abcd);
    rcc.BDCR &= ~RCC_BDCR_LSERDY; // Ready status may settle after clock restoration.
    assert(RtcClock::synchronize());
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
    const unsigned long beforeRead = ticks;
    assert(!RtcClock::readSeconds(seconds) && ticks - beforeRead < 5);
    assert(seconds == 0x1234abcd && backup.DR1 == 0x5254);
    synchronization = true;
    assert(RtcClock::synchronize()); // Wake restores register synchronization only.
    assert(RtcClock::readSeconds(seconds) && seconds == 0x1234abcd);
    for (unsigned int wake = 0; wake < 100; ++wake)
    {
        rtc.CRL &= ~RTC_CRL_RSF;
        rcc.BDCR &= ~RCC_BDCR_LSERDY;
        RtcClock::requestSynchronization();
        uint32_t epoch = 0;
        bool ready = false;
        for (int step = 0; step < 10 && !ready; ++step)
        {
            millis(); // Hardware progresses between nonblocking calls.
            const auto beforePoll = ticks;
            ready = RtcClock::pollSeconds(seconds, epoch);
            assert(ticks == beforePoll);
        }
        assert(ready);
        assert(RtcClock::readSeconds(seconds) && seconds == 0x1234abcd);
        assert(backup.DR1 == 0x5254 && rtc.PRLL == 32767);
    }
    backup.DR1 = 0;
    rcc.BDCR = 0;
    oscillator = false;
    const unsigned long beforeStart = ticks;
    assert(!RtcClock::begin() && ticks - beforeStart <= 5002);
    assert(backup.DR1 == 0);
    uint32_t epoch = 0;
    const auto beforePoll = ticks;
    for (int attempt = 0; attempt < 100; ++attempt)
        assert(!RtcClock::pollSeconds(seconds, epoch));
    assert(ticks == beforePoll); // Missing LSE never spins in the live loop.
    rcc.BDCR = 0; // Another power disturbance while initialization is pending.
    rtc.CRL = 0;
    assert(!RtcClock::pollSeconds(seconds, epoch));
    assert(rcc.BDCR & RCC_BDCR_LSEON);
    oscillator = true;
    bool ready = false;
    for (int step = 0; step < 10 && !ready; ++step)
    {
        millis();
        ready = RtcClock::pollSeconds(seconds, epoch);
    }
    assert(ready && seconds == 0 && epoch >= 2 && backup.DR1 == 0x5254);
    assert(rtc.PRLL == 32767);
    puts("[PASS] RTC startup, 100 nonblocking wake recoveries, lost backup-domain recovery and bounded failures");
}
