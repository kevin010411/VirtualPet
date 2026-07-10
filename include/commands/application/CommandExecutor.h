#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "pet/application/PetActionController.h"

struct CommandResult
{
    bool executed = false;
    AppCommandId commandId = AppCommandId::None;
    AnimationId layoutId = AnimationId::None;
    bool requestedOutfit = false;
    bool requestedSpecies = false;
    bool requestedMinigame = false;
};

class CommandExecutor : public CommandHost
{
public:
    CommandExecutor(PetActionController &petActions, AnimationController &animations);

    void begin(AppCommandId commandId);
    CommandResult complete(bool executed);

private:
    static constexpr unsigned long gameTick = 2000;
    static constexpr int maxFortune = 11;

    PetActionController &petActions;
    AnimationController &animations;
    CommandResult currentResult = {};

    static AnimationId fortuneToAnimationId(int fortuneIndex);
    void queuePostCommandHappyAnimation();
    void queueGiftAnimation();
    void queueStatusAnimation();
    void showStatusNotFound();
    bool queueStatusDirectAnimation();
    bool queueStatusSingleMeterAnimation();
    bool queueStatusRandomMetersAnimation();
    bool queueStatusTripleMeterAnimation();
    bool queueCompositeStatusAnimation();
    AnimationId compositeStatusAnimationId() const;
    bool canPlayGuessItemGame() const;

    bool commandHasAnimation(AnimationId id) const override;
    bool commandCanStatus() const override;
    AnimationId commandCurrentAgeAnimation() const override;
    void commandClearCommandAnimations() override;
    void commandFeedPet() override;
    void commandPredict() override;
    void commandGift() override;
    void commandMedicine() override;
    void commandShower() override;
#if ENABLE_GUESS_ITEM_GAME
    void commandHaveFun() override;
#endif
    void commandClean() override;
    void commandChangeOutfit() override;
    void commandChangeSpecies() override;
    void commandStatus() override;
};

#endif // COMMAND_EXECUTOR_H
