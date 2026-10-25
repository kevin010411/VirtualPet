#include <cassert>
#include <cstdio>
#include "pet/PetClock.h"
#include "platform/PowerController.h"
#include "platform/SessionClock.h"
#include "platform/VoltageMonitor.h"

static void testVoltagePolicy()
{
    using State = VoltageState;
    using Wake = VoltageMonitor::WakeResult;
    VoltageMonitor voltage;
    voltage.begin(100, State::Normal);
    assert(!voltage.sampleDue(199));
    assert(voltage.sampleDue(200));
    voltage.observe(200, State::Dimmed);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(300, State::Critical);
    assert(voltage.isCritical());
    assert(!voltage.shouldStop(5299));
    assert(voltage.shouldStop(5300));

    // Even a normal measurement recovers only one brightness level per window.
    voltage.observe(5400, State::Normal);
    assert(!voltage.shouldStop(10400));
    voltage.observe(10399, State::Normal);
    assert(voltage.isCritical());
    voltage.observe(10400, State::Normal);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(15399, State::Normal);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(15400, State::Normal);
    assert(voltage.state() == State::Normal);

    // An equal/worse sample cancels a partial recovery and restarts the window.
    voltage.observe(15500, State::Critical);
    voltage.observe(15600, State::Normal);
    voltage.observe(20500, State::Critical);
    voltage.observe(20600, State::Normal);
    voltage.observe(25599, State::Normal);
    assert(voltage.isCritical());
    voltage.observe(25600, State::Normal);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(25700, State::Dimmed);
    voltage.observe(25800, State::Normal);
    voltage.observe(30799, State::Normal);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(30800, State::Normal);
    assert(voltage.state() == State::Normal);

    VoltageMonitor bootCritical;
    bootCritical.begin(10, State::Critical);
    assert(!bootCritical.shouldStop(10000)); // First periodic sample starts delay.
    bootCritical.observe(110, State::Critical);
    assert(!bootCritical.shouldStop(5109));
    assert(bootCritical.shouldStop(5110));

    // A dark wake never brightens immediately; critical requires confirmation.
    assert(voltage.observeWake(State::Dimmed) == Wake::Ready);
    assert(voltage.state() == State::Dimmed);
    assert(voltage.observeWake(State::Normal) == Wake::Ready);
    assert(voltage.state() == State::Dimmed);
    assert(voltage.observeWake(State::Critical) == Wake::Retry);
    assert(voltage.isCritical());
    assert(voltage.observeWake(State::Normal) == Wake::ConfirmRecovery);
    assert(voltage.isCritical()); // Failed confirmation leaves state unchanged.
    assert(voltage.observeWake(State::Dimmed) == Wake::ConfirmRecovery);
    voltage.completeWakeRecovery(State::Normal, 40000);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(44999, State::Normal);
    assert(voltage.state() == State::Dimmed);
    voltage.observe(45000, State::Normal);
    assert(voltage.state() == State::Normal);

    // The final PVD resample may fall again after a successful confirmation.
    assert(voltage.observeWake(State::Critical) == Wake::Retry);
    voltage.completeWakeRecovery(State::Critical, 46000);
    assert(voltage.state() == State::Dimmed);
    assert(!voltage.shouldStop(60000));
    voltage.observe(46100, State::Critical);
    assert(voltage.isCritical());
    assert(!voltage.shouldStop(51099));
    assert(voltage.shouldStop(51100));

    VoltageMonitor wrapped;
    wrapped.begin(UINT32_MAX - 50, State::Normal);
    assert(!wrapped.sampleDue(48));
    assert(wrapped.sampleDue(49));
    const uint32_t criticalAt = UINT32_MAX - 900;
    wrapped.observe(criticalAt, State::Critical);
    assert(!wrapped.shouldStop(uint32_t(criticalAt + 4999U)));
    assert(wrapped.shouldStop(uint32_t(criticalAt + 5000U)));
    const uint32_t recoveredAt = UINT32_MAX - 500;
    wrapped.observe(recoveredAt, State::Normal);
    wrapped.observe(uint32_t(recoveredAt + 4999U), State::Normal);
    assert(wrapped.isCritical());
    wrapped.observe(uint32_t(recoveredAt + 5000U), State::Normal);
    assert(wrapped.state() == State::Dimmed);

    // Preserve the original zero sentinel when a sample lands exactly at wrap.
    VoltageMonitor zero;
    zero.observe(0, State::Critical);
    assert(!zero.shouldStop(5000));
    zero.observe(100, State::Critical);
    assert(zero.shouldStop(5100));
    zero.completeWakeRecovery(State::Normal, 0);
    zero.observe(100, State::Normal);
    zero.observe(5099, State::Normal);
    assert(zero.state() == State::Dimmed);
    zero.observe(5100, State::Normal);
    assert(zero.state() == State::Normal);
    puts("[PASS] Voltage sampling, critical STOP delay, staged recovery, dark wake and uint32 rollover");
}

int main()
{
    testVoltagePolicy();
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
    clock.consume(100); // Batched consumption clamps to backlog across rollover.
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
