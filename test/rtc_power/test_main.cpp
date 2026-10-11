#include <cassert>
#include <cstdio>
#include "pet/PetClock.h"
#include "platform/PowerController.h"

int main()
{
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
    puts("[PASS] RTC boundaries, remainder, rollover; idle/voltage brightness and sleep guards");
}
