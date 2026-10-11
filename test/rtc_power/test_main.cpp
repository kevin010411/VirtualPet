#include <cassert>
#include <cstdio>
#include "pet/PetClock.h"
#include "platform/PowerController.h"
#include "platform/SessionClock.h"

int main()
{
    SessionClock session;
    assert(session.update(0, true, 100, 1) == 0);
    assert(session.update(1000, true, 101, 1) == 1);
    assert(session.update(1001, true, 1901, 1) == 1801); // STOP: millis barely advances.
    assert(session.update(1500, false, 0, 0) == 1801);
    assert(session.update(4001, false, 0, 0) == 1804);
    assert(session.update(6001, true, 1906, 1) == 1806); // No double count of fallback time.
    assert(session.update(8001, false, 0, 0) == 1808);
    assert(session.update(8002, true, 0, 2) == 1808); // Backup loss: no huge unsigned catch-up.
    assert(session.update(9002, true, 1, 2) == 1809);
    SessionClock missingAtBoot;
    assert(missingAtBoot.update(100, false, 0, 0) == 0);
    assert(missingAtBoot.update(2100, false, 0, 0) == 2);
    assert(missingAtBoot.update(2101, true, 0, 1) == 2);
    assert(missingAtBoot.update(3101, true, 1, 1) == 3);
    SessionClock wrapped;
    assert(wrapped.update(UINT32_MAX - 500, true, UINT32_MAX, 1) == 0);
    assert(wrapped.update(499, false, 0, 0) == 1);
    assert(wrapped.update(500, true, 0, 1) == 1);
    assert(wrapped.update(1500, true, 1, 1) == 2);
    PetClock clock;
    clock.reset(100);
    clock.observe(101);
    assert(!clock.pending());
    clock.observe(105);
    assert(clock.due() == 2);
    clock.observe(109); // Blocked work keeps the original snapshot.
    assert(clock.due() == 2);
    clock.consume();
    clock.consume();
    clock.observe(105);
    assert(!clock.pending());
    clock.observe(106);
    assert(clock.due() == 1);
    clock.reset(UINT32_MAX - 1);
    clock.observe(2);
    assert(clock.due() == 2);
    clock.consume();
    clock.consume();
    clock.observe(3);
    assert(!clock.pending());
    clock.observe(4);
    assert(clock.due() == 1);

    // Catalog and detail browsing preserve the partial day even across STOP.
    clock.reset(100);
    clock.observe(101);
    clock.pause(101);
    clock.observe(20000);
    clock.pause(20000); // Re-entering detail must not move the pause baseline.
    assert(!clock.pending());
    clock.resume(20001);
    clock.resume(20001);
    clock.observe(20001);
    assert(!clock.pending());
    clock.observe(20002);
    assert(clock.due() == 1);
    clock.consume();
    clock.observe(20003);
    assert(!clock.pending());
    clock.reset(UINT32_MAX - 2);
    clock.pause(UINT32_MAX - 1);
    clock.resume(100);
    clock.observe(100);
    assert(!clock.pending());
    clock.observe(101);
    assert(clock.due() == 1);
    clock.pause(101);
    clock.reset(500); // A full reset clears a catalog pause too.
    clock.observe(502);
    assert(clock.due() == 1);

    PowerController power;
    power.interact(100);
    assert(power.brightness(30099, 255) == 255);
    assert(power.brightness(30100, 255) == 64);
    assert(power.brightness(30100, 128) == 64);
    assert(power.brightness(30100, 26) == 26);
    assert(!power.shouldSleep(120099, false));
    assert(power.shouldSleep(120100, false));
    assert(!power.shouldSleep(120100, true));
    power.interact(120100);
    assert(power.brightness(120100, 128) == 128);
    assert(!power.shouldSleep(120100, false));
    power.interact(UINT32_MAX - 100);
    assert(power.brightness(29900, 255) == 64);
    assert(power.shouldSleep(119900, false));
    puts("[PASS] RTC/awake fallback without double count, counter loss, rollover; pet-day boundaries and sleep guards");
}
