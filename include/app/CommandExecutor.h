#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "app/AnimationController.h"
#include "app/CommandController.h"
#include "app/PetActionController.h"

struct CommandResult
{
    bool executed = false;
    AppCommandId commandId = AppCommandId::None;
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
    bool queueStatusDirectAnimation();
    bool queueStatusAgeAnimation();
    bool queueStatusRandom3Animation();
    bool canPlayGuessItemGame() const;

    bool commandHasAnimation(AnimationId id) const override;
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
