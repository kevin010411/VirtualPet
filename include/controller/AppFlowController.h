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
    Appearance,
    Collection,
    Evolution,
};

class AppFlowController
{
public:
    bool is(AppStage stage) const { return currentStage == stage; }
    void enter(AppStage stage) { currentStage = stage; }
    bool allowsPetTime() const;
    bool allowsMenuNavigation() const;
    void beginEvolution();
    void endEvolution();
    void enterBattery();
    void leaveBattery();

private:
    AppStage currentStage = AppStage::Command;
    AppStage stageBeforeBattery = AppStage::Command;
    AppStage stageBeforeEvolution = AppStage::Command;
};

#endif // APP_FLOW_CONTROLLER_H
