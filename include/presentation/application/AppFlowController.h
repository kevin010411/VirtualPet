#ifndef APP_FLOW_CONTROLLER_H
#define APP_FLOW_CONTROLLER_H

#include <Arduino.h>
#include "shared/config/AppProfile.h"
#include "commands/application/CommandController.h"

enum class AppStage : uint8_t
{
    FirstLaunch,
    Startup,
    Command,
    Minigame,
    Battery,
    FatalError,
};

class AppFlowController
{
public:
    AppStage stage() const;
    AppCommandId firstLaunchRequiredCommand() const;
    bool isFirstLaunch() const;
    bool isStartup() const;
    bool isCommand() const;
    bool isMinigame() const;
    bool isBattery() const;
    bool isFatalError() const;

    void beginFirstLaunch();
    void enterCommand();
    void enterMinigame();
    void onMinigameEnded();
    void enterBattery();
    void leaveBattery();
    void enterFatalError();
    bool requestStartup();
    bool completeFirstLaunch(AppCommandId completedCommand);

private:
    static constexpr AppCommandId requiredCommand =
        static_cast<AppCommandId>(APP_FIRST_LAUNCH_REQUIRED_COMMAND);

    AppStage currentStage = AppStage::Command;
    AppStage stageBeforeBattery = AppStage::Command;
};

#endif // APP_FLOW_CONTROLLER_H
