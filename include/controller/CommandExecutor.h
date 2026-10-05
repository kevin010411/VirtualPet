#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "animation/AnimationController.h"
#include "controller/CommandController.h"
#include "pet/PetBehaviorRuntime.h"

class Pet;

struct PetBehaviorConfig;
class PetBehaviorRuntime;

struct CommandResult
{
    bool executed = false;
    AppCommandId commandId = AppCommandId::None;
    PetBehaviorActionResult actionResult = PetBehaviorActionResult::Rejected;
    bool requestedOutfit = false;
    bool requestedMinigame = false;
    bool resourceError = false;
};

class CommandExecutor
{
public:
    CommandExecutor(const Pet &pet,
                    AnimationController &animations,
                    PetBehaviorRuntime &petBehaviorRuntime);

    CommandResult execute(const CommandController &commands);
    void configureRuntimeContract(const PetBehaviorConfig &config);

private:
    static constexpr unsigned long gameTick = 2000;
    static constexpr int maxFortune = 11;

    const Pet &pet;
    AnimationController &animations;
    PetBehaviorRuntime &petBehaviorRuntime;
    const PetBehaviorConfig *petBehaviorConfig = nullptr;
#if ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
    uint8_t nextStatusSetIndex = 0;
#endif

#if ENABLE_COMMAND_PREDICT
    static FirmwarePlaybackRole fortuneToPlaybackRole(int fortuneIndex);
#endif
    bool queueStatusSetsAnimation();
#if ENABLE_GUESS_GAME
    bool canPlayGuessItemGame() const;
#endif

#if ENABLE_COMMAND_PREDICT
    bool canPredict() const;
    void commandPredict(CommandResult &result);
#endif
};

#endif // COMMAND_EXECUTOR_H
