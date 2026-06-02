#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <Arduino.h>

typedef void (*ButtonCallback)();

class ButtonInput
{
public:
    ButtonInput(int previousPin, int nextPin, int confirmPin, unsigned long cooldownMs);

    void begin();
    void clearFlags();
    bool hasPendingPress() const;
    bool isPreviousNextComboHeld() const;

    void update(bool isSleeping,
                ButtonCallback onPrevious,
                ButtonCallback onNext,
                ButtonCallback onConfirm,
                ButtonCallback onWake,
                ButtonCallback onAnyPress);
    void handleConfirmLongPress(unsigned long thresholdMs, ButtonCallback callback);
    void handlePreviousNextComboLongPress(unsigned long thresholdMs, ButtonCallback callback);

private:
    static ButtonInput *activeInstance;
    static void handlePreviousInterrupt();
    static void handleNextInterrupt();
    static void handleConfirmInterrupt();

    void notePreviousInterrupt();
    void noteNextInterrupt();
    void noteConfirmInterrupt();
    void handleLongPress(int pin, unsigned long thresholdMs, ButtonCallback callback);

    int previousPin;
    int nextPin;
    int confirmPin;
    unsigned long cooldownMs;
    unsigned long lastPreviousPressTime = 0;
    unsigned long lastNextPressTime = 0;
    unsigned long lastConfirmPressTime = 0;
    unsigned long longPressStartTime[20] = {};
    bool longPressFired[20] = {};
    unsigned long comboStartTime = 0;
    bool comboFired = false;
    volatile bool previousPressed = false;
    volatile bool nextPressed = false;
    volatile bool confirmPressed = false;
    volatile bool anyPressPending = false;
};

#endif
