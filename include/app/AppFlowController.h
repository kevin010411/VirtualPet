#ifndef APP_FLOW_CONTROLLER_H
#define APP_FLOW_CONTROLLER_H

#include <Arduino.h>
#include "app/AppProfile.h"
#include "app/CommandController.h"

enum class AppStage : uint8_t
{
    FirstLaunch,
    Startup,
    Command,
    Minigame,
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

    void beginFirstLaunch();
    void enterCommand();
    void enterMinigame();
    void onMinigameEnded();
    bool requestStartup();
    bool completeFirstLaunch(AppCommandId completedCommand);

private:
    static constexpr AppCommandId requiredCommand =
        static_cast<AppCommandId>(APP_FIRST_LAUNCH_REQUIRED_COMMAND);

    AppStage currentStage = AppStage::Command;
};

#endif // APP_FLOW_CONTROLLER_H
