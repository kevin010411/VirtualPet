#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "animation/AnimationController.h"
#include "controller/CommandController.h"
#include "pet/PetBehaviorRuntime.h"

class Pet;

struct PetBehaviorConfig;
struct StatusSetsConfig;
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
    Shop,
    Inventory,
    AdvanceDay,
    Menu,
    CaptureGame,
    InstructionGame,
};

struct CommandResult
{
    CommandResultKind kind = CommandResultKind::None;
    bool resourceError = false;
#if ENABLE_PROJECT_MENUS
    uint8_t menuSlot = 0;
#endif
};

class CommandExecutor
{
public:
    CommandExecutor(const Pet &pet,
                    AnimationController &animations,
                    PetBehaviorRuntime &petBehaviorRuntime);

    CommandResult execute(const CommandController &commands);
#if ENABLE_PROJECT_MENUS
    CommandResult executeSystemCommand(RuntimeSystemCommandId command);
    CommandResult executeUserAction(uint8_t actionSlot);
#endif
    void configureRuntimeContract(const StatusSetsConfig &statusSets, const PetBehaviorConfig &behavior);

private:
    const Pet &pet;
    AnimationController &animations;
    PetBehaviorRuntime &petBehaviorRuntime;
    const StatusSetsConfig *statusSets = nullptr;
    uint16_t activeStatMask = 0;
    bool manualDays = false;
    CommandResult execute(AppCommandId id, uint8_t targetSlot);
#if ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
    uint8_t nextStatusSetIndex = 0;
#endif

    bool queueStatusSetsAnimation();
#if ENABLE_GUESS_GAME
    bool canPlayGuessItemGame() const;
#endif

};

#endif // COMMAND_EXECUTOR_H
