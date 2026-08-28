#include "presentation/application/AppFlowController.h"

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

bool AppFlowController::isBattery() const
{
    return currentStage == AppStage::Battery;
}

bool AppFlowController::isFatalError() const
{
    return currentStage == AppStage::FatalError;
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

void AppFlowController::enterBattery()
{
    if (currentStage == AppStage::Battery || currentStage == AppStage::FatalError)
        return;
    stageBeforeBattery = currentStage;
    currentStage = AppStage::Battery;
}

void AppFlowController::leaveBattery()
{
    if (currentStage == AppStage::Battery)
        currentStage = stageBeforeBattery;
}

void AppFlowController::enterFatalError()
{
    currentStage = AppStage::FatalError;
}

bool AppFlowController::requestStartup()
{
    if (currentStage == AppStage::Battery || currentStage == AppStage::FatalError)
        return false;
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
