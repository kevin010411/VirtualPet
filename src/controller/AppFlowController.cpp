#include "controller/AppFlowController.h"

bool AppFlowController::allowsPetTime() const
{
    return is(AppStage::Command) || is(AppStage::Minigame) || is(AppStage::Appearance) || is(AppStage::Evolution);
}

bool AppFlowController::allowsMenuNavigation() const
{
    return is(AppStage::Command) || is(AppStage::Appearance) || is(AppStage::Collection);
}

void AppFlowController::beginEvolution()
{
    if (is(AppStage::Evolution) || is(AppStage::Battery) || is(AppStage::FatalError)) return;
    stageBeforeEvolution = currentStage;
    enter(AppStage::Evolution);
}

void AppFlowController::endEvolution()
{
    if (is(AppStage::Evolution)) enter(stageBeforeEvolution);
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
