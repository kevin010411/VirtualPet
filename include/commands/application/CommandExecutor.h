#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "pet/application/PetActionController.h"

struct PetBehaviorConfig;

struct CommandResult
{
    bool executed = false;
    AppCommandId commandId = AppCommandId::None;
    AnimationId layoutId = AnimationId::None;
    bool requestedOutfit = false;
    bool requestedSpecies = false;
    bool requestedMinigame = false;
    bool resourceError = false;
};

class CommandExecutor : public CommandHost
{
public:
    CommandExecutor(PetActionController &petActions, AnimationController &animations);

    void begin(AppCommandId commandId);
    CommandResult complete(bool executed);
    void configureRuntimeContract(const PetBehaviorConfig &config);

private:
    static constexpr unsigned long gameTick = 2000;
    static constexpr int maxFortune = 11;

    PetActionController &petActions;
    AnimationController &animations;
    const PetBehaviorConfig *petBehaviorConfig = nullptr;
    CommandResult currentResult = {};
#if ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
    uint8_t nextStatusSetIndex = 0;
#endif

#if ENABLE_COMMAND_PREDICT
    static AnimationId fortuneToAnimationId(int fortuneIndex);
#endif
    void queueStatusAnimation();
    bool queueStatusSetsAnimation();
#if ENABLE_GUESS_GAME
    bool canPlayGuessItemGame() const;
#endif

    bool commandHasAnimation(AnimationId id) const override;
    bool commandCanStatus() const override;
#if ENABLE_COMMAND_PREDICT
    void commandPredict() override;
#endif
#if ENABLE_GUESS_GAME
    void commandGuessGame() override;
#endif
#if ENABLE_COMMAND_OUTFIT
    void commandChangeOutfit() override;
#endif
#if ENABLE_COMMAND_SPECIES
    void commandChangeSpecies() override;
#endif
    void commandStatus() override;
};

#endif // COMMAND_EXECUTOR_H
