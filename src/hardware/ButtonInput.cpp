#include "hardware/ButtonInput.h"

ButtonInput *ButtonInput::activeInstance = nullptr;

ButtonInput::ButtonInput(int previousPinValue, int nextPinValue, int confirmPinValue, unsigned long cooldownMsValue)
    : previousPin(previousPinValue),
      nextPin(nextPinValue),
      confirmPin(confirmPinValue),
      cooldownMs(cooldownMsValue)
{
}

void ButtonInput::begin()
{
    activeInstance = this;

    pinMode(previousPin, INPUT_PULLUP);
    pinMode(confirmPin, INPUT_PULLUP);
    pinMode(nextPin, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(confirmPin), handleConfirmInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(nextPin), handleNextInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(previousPin), handlePreviousInterrupt, FALLING);
}

void ButtonInput::clearFlags()
{
    previousPressed = false;
    confirmPressed = false;
    nextPressed = false;
}

bool ButtonInput::hasPendingPress() const
{
    return previousPressed || confirmPressed || nextPressed;
}

bool ButtonInput::isPreviousNextComboHeld() const
{
    return digitalRead(previousPin) == LOW && digitalRead(nextPin) == LOW;
}

void ButtonInput::update(bool isSleeping,
                         ButtonCallback onPrevious,
                         ButtonCallback onNext,
                         ButtonCallback onConfirm,
                         ButtonCallback onWake)
{
    const unsigned long now = millis();

    if (isPreviousNextComboHeld())
    {
        previousPressed = false;
        nextPressed = false;
        return;
    }

    if (isSleeping)
    {
        if (hasPendingPress() && onWake != nullptr)
            onWake();
        return;
    }

    if (previousPressed)
    {
        if (now - lastPreviousPressTime >= cooldownMs && onPrevious != nullptr)
        {
            onPrevious();
            lastPreviousPressTime = now;
        }
        previousPressed = false;
    }

    if (nextPressed)
    {
        if (now - lastNextPressTime >= cooldownMs && onNext != nullptr)
        {
            onNext();
            lastNextPressTime = now;
        }
        nextPressed = false;
    }

    if (confirmPressed)
    {
        if (now - lastConfirmPressTime >= cooldownMs && onConfirm != nullptr)
        {
            onConfirm();
            lastConfirmPressTime = now;
        }
        confirmPressed = false;
    }
}

void ButtonInput::handleConfirmLongPress(unsigned long thresholdMs, ButtonCallback callback)
{
    handleLongPress(confirmPin, thresholdMs, callback);
}

void ButtonInput::handlePreviousNextComboLongPress(unsigned long thresholdMs, ButtonCallback callback)
{
    const bool bothPressed = isPreviousNextComboHeld();

    if (bothPressed)
    {
        previousPressed = false;
        nextPressed = false;
        if (comboStartTime == 0)
            comboStartTime = millis();

        if (!comboFired && (millis() - comboStartTime > thresholdMs))
        {
            if (callback != nullptr)
                callback();
            comboFired = true;
        }
    }
    else
    {
        comboStartTime = 0;
        comboFired = false;
    }
}

void ButtonInput::handlePreviousInterrupt()
{
    if (activeInstance != nullptr)
        activeInstance->notePreviousInterrupt();
}

void ButtonInput::handleNextInterrupt()
{
    if (activeInstance != nullptr)
        activeInstance->noteNextInterrupt();
}

void ButtonInput::handleConfirmInterrupt()
{
    if (activeInstance != nullptr)
        activeInstance->noteConfirmInterrupt();
}

void ButtonInput::notePreviousInterrupt()
{
    if (digitalRead(previousPin) == HIGH)
        return;
    previousPressed = true;
}

void ButtonInput::noteNextInterrupt()
{
    if (digitalRead(nextPin) == HIGH)
        return;
    nextPressed = true;
}

void ButtonInput::noteConfirmInterrupt()
{
    if (digitalRead(confirmPin) == HIGH)
        return;
    confirmPressed = true;
}

void ButtonInput::handleLongPress(int pin, unsigned long thresholdMs, ButtonCallback callback)
{
    const int idx = pin % 20;

    if (digitalRead(pin) == LOW)
    {
        if (longPressStartTime[idx] == 0)
            longPressStartTime[idx] = millis();

        if (!longPressFired[idx] && (millis() - longPressStartTime[idx] > thresholdMs))
        {
            if (callback != nullptr)
                callback();
            longPressFired[idx] = true;
        }
    }
    else
    {
        longPressStartTime[idx] = 0;
        longPressFired[idx] = false;
    }
}
