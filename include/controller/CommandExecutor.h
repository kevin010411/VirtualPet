#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "animation/AnimationController.h"
#include "controller/CommandController.h"
#include "pet/PetBehaviorRuntime.h"

class Pet;

struct PetBehaviorConfig;
class PetBehaviorRuntime;

enum class CommandResultKind : uint8_t
{
    None,
    UserAction,
    Playback,
    Outfit,
    Collection,
    Guess,
    Predict,
    Instruction,
};

struct CommandResult
{
    CommandResultKind kind = CommandResultKind::None;
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
    const Pet &pet;
    AnimationController &animations;
    PetBehaviorRuntime &petBehaviorRuntime;
    const PetBehaviorConfig *petBehaviorConfig = nullptr;
#if ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
    uint8_t nextStatusSetIndex = 0;
#endif

    bool queueStatusSetsAnimation();
#if ENABLE_GUESS_GAME
    bool canPlayGuessItemGame() const;
#endif

};

#endif // COMMAND_EXECUTOR_H
