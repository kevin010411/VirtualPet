#include "app/AppFlowController.h"

AppStage AppFlowController::stage() const
{
    return currentStage;
}

AppCommandId AppFlowController::firstLaunchRequiredCommand() const
{
    return requiredCommand;
}

bool AppFlowController::isFirstLaunch() const
{
    return currentStage == AppStage::FirstLaunch;
}

bool AppFlowController::isStartup() const
{
    return currentStage == AppStage::Startup;
}

bool AppFlowController::isCommand() const
{
    return currentStage == AppStage::Command;
}

bool AppFlowController::isMinigame() const
{
    return currentStage == AppStage::Minigame;
}

void AppFlowController::beginFirstLaunch()
{
    currentStage = AppStage::FirstLaunch;
}

void AppFlowController::enterCommand()
{
    currentStage = AppStage::Command;
}

void AppFlowController::enterMinigame()
{
    currentStage = AppStage::Minigame;
}

void AppFlowController::onMinigameEnded()
{
    enterCommand();
}

bool AppFlowController::requestStartup()
{
    currentStage = AppStage::Startup;
    return true;
}

bool AppFlowController::completeFirstLaunch(AppCommandId completedCommand)
{
    if (currentStage != AppStage::FirstLaunch || completedCommand != requiredCommand)
        return false;

    enterCommand();
    return false;
}
