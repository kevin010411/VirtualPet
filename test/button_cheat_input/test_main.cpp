#include <cassert>
#include "platform/hardware/ButtonInput.h"

static unsigned long clockMs = 1000;
static int pins[3] = {HIGH, HIGH, HIGH};
static int shorts[3] = {};
static int beeps = 0;

unsigned long millis() { return clockMs; }
int digitalRead(int pin) { return pins[pin]; }
void pinMode(int, int) {}
int digitalPinToInterrupt(int pin) { return pin; }
void attachInterrupt(int, void (*)(), int) {}

void previous() { ++shorts[0]; }
void next() { ++shorts[1]; }
void confirm() { ++shorts[2]; }
void beep() { ++beeps; }

static CheatButtonEvent tick(ButtonInput &input, unsigned long ms = 40)
{
    clockMs += ms;
    return input.update(previous, next, confirm, beep);
}

static void press(ButtonInput &input, int pin)
{
    pins[pin] = LOW;
    tick(input);
    tick(input);
}

static void releaseAll(ButtonInput &input)
{
    pins[0] = pins[1] = pins[2] = HIGH;
    tick(input);
    tick(input);
}

int main()
{
    ButtonInput input(0, 1, 2, 250);
    input.begin();

    press(input, 2);
    assert(shorts[2] == 0);
    releaseAll(input);
    assert(shorts[2] == 1 && beeps == 1);

    press(input, 0);
    assert(tick(input, 2010) == CheatButtonEvent::None);
    releaseAll(input);
    assert(shorts[0] == 0);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    assert(shorts[0] == 0 && shorts[1] == 0 && shorts[2] == 1);

    press(input, 1);
    releaseAll(input);
    assert(shorts[1] == 1);
    // A short press exits cheat mode; a later long press has no cheat action.
    press(input, 1);
    assert(tick(input, 2010) == CheatButtonEvent::None);
    releaseAll(input);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    // main.cpp routes previousPin to OnRightKey: it is the physical right key.
    press(input, 0);
    assert(tick(input, 2010) == CheatButtonEvent::SetStageDays);
    assert(tick(input, 100) == CheatButtonEvent::None);
    releaseAll(input);
    assert(shorts[0] == 0);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    // nextPin routes to OnLeftKey: it is the physical left key.
    press(input, 1);
    assert(tick(input, 2010) == CheatButtonEvent::ResetPet);
    releaseAll(input);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::RestartTft);
    releaseAll(input);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    assert(tick(input, 30001) == CheatButtonEvent::Exited);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::None);
    releaseAll(input);
    assert(shorts[2] == 1);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    press(input, 0);
    pins[0] = HIGH;
    tick(input);
    assert(tick(input) == CheatButtonEvent::Exited);
    assert(shorts[0] == 1);

    press(input, 0);
    press(input, 1);
    press(input, 2);
    assert(tick(input, 2010) == CheatButtonEvent::Entered);
    releaseAll(input);
    press(input, 2);
    pins[2] = HIGH;
    tick(input);
    assert(tick(input) == CheatButtonEvent::Exited);
    assert(shorts[2] == 2);
}
