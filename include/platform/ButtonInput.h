#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <Arduino.h>

typedef void (*ButtonCallback)();

enum class CheatButtonEvent : uint8_t
{
    None, Entered, Exited, RestartTft, ResetPet, SetStageDays,
};

class ButtonInput
{
public:
    ButtonInput(int previousPin, int nextPin, int confirmPin, unsigned long cooldownMs = 250);
    void begin();
    void clearFlags();
    bool takeDeathPress();
    bool isHeld() const { return readMask() != 0; }
    bool isDebouncedHeld() const { return stableMask != 0; }
    uint32_t wakeGeneration() const { return wakeCount; }
    CheatButtonEvent update(ButtonCallback onPrevious, ButtonCallback onNext,
                            ButtonCallback onConfirm, ButtonCallback onAnyPress);

private:
    static void handleWakeInterrupt();
    static volatile uint32_t wakeCount;
    uint8_t readMask() const;
    CheatButtonEvent handleStableInput(unsigned long now, ButtonCallback onPrevious,
                                       ButtonCallback onNext, ButtonCallback onConfirm,
                                       ButtonCallback onAnyPress);

    int previousPin;
    int nextPin;
    int confirmPin;
    unsigned long cooldownMs;
    unsigned long lastShortPressAt = 0;
    unsigned long holdStartedAt = 0;
    unsigned long gestureStartedAt = 0;
    unsigned long modeActivityAt = 0;
    uint8_t stableMask = 0;
    uint8_t gestureMask = 0;
    bool waitForRelease = false;
    bool cheatMode = false;
};

#endif
