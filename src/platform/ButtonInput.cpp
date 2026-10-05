#include "platform/ButtonInput.h"

namespace
{
constexpr uint8_t kPrevious = 1;
constexpr uint8_t kNext = 2;
constexpr uint8_t kConfirm = 4;
constexpr uint8_t kAll = kPrevious | kNext | kConfirm;
constexpr unsigned long kDebounceMs = 30;
constexpr unsigned long kLongPressMs = 2000;
constexpr unsigned long kModeTimeoutMs = 30000;
}

ButtonInput::ButtonInput(int previousPinValue, int nextPinValue, int confirmPinValue, unsigned long cooldownMsValue)
    : previousPin(previousPinValue), nextPin(nextPinValue),
      confirmPin(confirmPinValue), cooldownMs(cooldownMsValue)
{
}

void ButtonInput::begin()
{
    pinMode(previousPin, INPUT_PULLUP);
    pinMode(confirmPin, INPUT_PULLUP);
    pinMode(nextPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(confirmPin), handleWakeInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(nextPin), handleWakeInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(previousPin), handleWakeInterrupt, FALLING);
}

void ButtonInput::clearFlags()
{
    rawMask = readMask();
    stableMask = rawMask;
    gestureMask = 0;
    holdStartedAt = 0;
    gestureStartedAt = 0;
    waitForRelease = rawMask != 0;
    cheatMode = false;
}

uint8_t ButtonInput::readMask() const
{
    return (digitalRead(previousPin) == LOW ? kPrevious : 0) |
           (digitalRead(nextPin) == LOW ? kNext : 0) |
           (digitalRead(confirmPin) == LOW ? kConfirm : 0);
}

CheatButtonEvent ButtonInput::update(ButtonCallback onPrevious,
                                     ButtonCallback onNext,
                                     ButtonCallback onConfirm,
                                     ButtonCallback onAnyPress)
{
    const unsigned long now = millis();
    const uint8_t observed = readMask();
    if (observed != rawMask)
    {
        rawMask = observed;
        rawChangedAt = now;
    }
    if (rawMask != stableMask && now - rawChangedAt >= kDebounceMs)
    {
        stableMask = rawMask;
        holdStartedAt = now;
        if (stableMask != 0)
        {
            if (gestureMask == 0)
                gestureStartedAt = now;
            gestureMask |= stableMask;
        }
    }
    return handleStableInput(now, onPrevious, onNext, onConfirm, onAnyPress);
}

CheatButtonEvent ButtonInput::handleStableInput(unsigned long now,
                                                ButtonCallback onPrevious,
                                                ButtonCallback onNext,
                                                ButtonCallback onConfirm,
                                                ButtonCallback onAnyPress)
{
    if (waitForRelease)
    {
        if (stableMask == 0)
        {
            waitForRelease = false;
            gestureMask = 0;
        }
        return CheatButtonEvent::None;
    }
    if (cheatMode && stableMask == 0 && now - modeActivityAt >= kModeTimeoutMs)
    {
        cheatMode = false;
        gestureMask = 0;
        return CheatButtonEvent::Exited;
    }
    if (stableMask != 0)
    {
        if (cheatMode)
            modeActivityAt = now;
        if (now - holdStartedAt < kLongPressMs)
            return CheatButtonEvent::None;
        if (stableMask == kAll && gestureMask == kAll)
        {
            cheatMode = !cheatMode;
            modeActivityAt = now;
            waitForRelease = true;
            return cheatMode ? CheatButtonEvent::Entered : CheatButtonEvent::Exited;
        }
        if (!cheatMode || gestureMask != stableMask)
            return CheatButtonEvent::None;
        CheatButtonEvent event = CheatButtonEvent::None;
        // Physical right is previousPin; normal input maps it to OnRightKey.
        if (stableMask == kPrevious)
            event = CheatButtonEvent::SetStageDays;
        else if (stableMask == kNext)
            event = CheatButtonEvent::ResetPet;
        else if (stableMask == kConfirm)
            event = CheatButtonEvent::RestartTft;
        if (event != CheatButtonEvent::None)
        {
            cheatMode = false;
            waitForRelease = true;
        }
        return event;
    }
    if (gestureMask != 0 && now - gestureStartedAt < kLongPressMs &&
        (lastShortPressAt == 0 || now - lastShortPressAt >= cooldownMs))
    {
        ButtonCallback callback = nullptr;
        if (gestureMask == kPrevious)
            callback = onPrevious;
        else if (gestureMask == kNext)
            callback = onNext;
        else if (gestureMask == kConfirm)
            callback = onConfirm;
        if (callback != nullptr)
        {
            const bool leavingCheatMode = cheatMode;
            cheatMode = false;
            if (onAnyPress != nullptr)
                onAnyPress();
            callback();
            lastShortPressAt = now;
            gestureMask = 0;
            return leavingCheatMode ? CheatButtonEvent::Exited : CheatButtonEvent::None;
        }
    }
    gestureMask = 0;
    return CheatButtonEvent::None;
}

// The interrupt wakes STOP mode; update() owns all button interpretation.
void ButtonInput::handleWakeInterrupt() {}
