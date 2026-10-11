#include <cassert>
#include "platform/ButtonInput.h"

static unsigned long clockMs = 1000;
static int pins[3] = {HIGH, HIGH, HIGH};
static int shorts[3] = {};
static int beeps = 0;
static void (*wakeInterrupt)() = nullptr;

unsigned long millis() { return clockMs; }
int digitalRead(int pin) { return pins[pin]; }
void pinMode(int, int) {}
int digitalPinToInterrupt(int pin) { return pin; }
void attachInterrupt(int, void (*callback)(), int) { wakeInterrupt = callback; }
void previous() { ++shorts[0]; }
void next() { ++shorts[1]; }
void confirm() { ++shorts[2]; }
void beep() { ++beeps; }

static CheatButtonEvent tick(ButtonInput &input, unsigned long ms = 1)
{
    clockMs += ms;
    return input.update(previous, next, confirm, beep);
}

static void releaseAll(ButtonInput &input)
{
    pins[0] = pins[1] = pins[2] = HIGH;
    tick(input);
}

static void enterCheat(ButtonInput &input)
{
    pins[0] = pins[1] = pins[2] = LOW;
    tick(input);
    assert(tick(input, 2000) == CheatButtonEvent::Entered);
    releaseAll(input);
}

int main()
{
    ButtonInput input(0, 1, 2, 250);
    input.begin();
    assert(wakeInterrupt != nullptr && input.wakeGeneration() == 0);
    wakeInterrupt();
    input.clearFlags();
    assert(input.wakeGeneration() == 1);

    for (int pin = 0; pin < 3; ++pin)
    {
        tick(input, 250);
        const int before = shorts[pin];
        const int beepsBefore = beeps;
        pins[pin] = LOW;
        tick(input); // Dispatch after 1 ms, before release or a 30 ms wait.
        assert(shorts[pin] == before + 1 && beeps == beepsBefore + 1);
        assert(input.isHeld() && input.isDebouncedHeld());
        tick(input, 2001); // Holding cannot repeat an ordinary command.
        assert(shorts[pin] == before + 1);
        releaseAll(input);
        assert(shorts[pin] == before + 1);
        assert(!input.isHeld() && !input.isDebouncedHeld());
    }

    const int confirmsBefore = shorts[2];
    pins[2] = LOW;
    tick(input);
    assert(shorts[2] == confirmsBefore + 1);
    releaseAll(input);
    pins[2] = LOW;
    tick(input);
    releaseAll(input);
    assert(shorts[2] == confirmsBefore + 1); // Keep the 250 ms interval.
    tick(input, 250);
    pins[2] = LOW;
    tick(input);
    assert(shorts[2] == confirmsBefore + 2);
    releaseAll(input);

    pins[2] = LOW;
    input.clearFlags(); // A wake press is consumed, including its release.
    tick(input, 250);
    releaseAll(input);
    assert(shorts[2] == confirmsBefore + 2);

    const CheatButtonEvent commands[] = {CheatButtonEvent::SetStageDays,
        CheatButtonEvent::ResetPet, CheatButtonEvent::RestartTft};
    for (int pin = 0; pin < 3; ++pin)
    {
        const int before = shorts[pin];
        enterCheat(input);
        pins[pin] = LOW;
        tick(input);
        assert(shorts[pin] == before);
        assert(tick(input, 2000) == commands[pin]);
        assert(tick(input, 100) == CheatButtonEvent::None);
        releaseAll(input);
        assert(shorts[pin] == before);
    }

    enterCheat(input);
    assert(tick(input, 30001) == CheatButtonEvent::Exited);
    enterCheat(input);
    const int previousBefore = shorts[0];
    pins[0] = LOW;
    tick(input);
    assert(shorts[0] == previousBefore);
    pins[0] = HIGH;
    assert(tick(input) == CheatButtonEvent::Exited);
    assert(shorts[0] == previousBefore + 1);
}
