#ifndef APP_FLOW_CONTROLLER_H
#define APP_FLOW_CONTROLLER_H

#include <Arduino.h>
#include "common/AppProfile.h"

enum class AppStage : uint8_t
{
    Startup = 1,
    Command,
    Minigame,
    Battery,
    FatalError,
};

class AppFlowController
{
public:
    AppStage stage() const;
    bool isStartup() const;
    bool isCommand() const;
    bool isMinigame() const;
    bool isBattery() const;
    bool isFatalError() const;

    void enterCommand();
    void enterMinigame();
    void onMinigameEnded();
    void enterBattery();
    void leaveBattery();
    void enterFatalError();
    bool requestStartup();

private:
    AppStage currentStage = AppStage::Command;
    AppStage stageBeforeBattery = AppStage::Command;
};

#endif // APP_FLOW_CONTROLLER_H
