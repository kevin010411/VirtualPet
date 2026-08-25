#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H

#include <Arduino.h>
#include <stddef.h>
#include "shared/config/AppProfile.h"
#include "animation/domain/Animation.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"

enum class AppCommandId : uint8_t
{
    None = APP_COMMAND_NONE,
    Predict = APP_COMMAND_PREDICT,
    GuessGame = APP_COMMAND_GUESS_GAME,
    ChangeOutfit = APP_COMMAND_CHANGE_OUTFIT,
    Status = APP_COMMAND_STATUS,
    ChangeSpecies = APP_COMMAND_CHANGE_SPECIES,
    UserAction = APP_COMMAND_USER_ACTION,
};

class CommandHost
{
public:
    virtual ~CommandHost() = default;

    virtual bool commandHasAnimation(AnimationId id) const = 0;
    virtual bool commandCanStatus() const = 0;
    virtual void commandClearCommandAnimations() = 0;

#if ENABLE_COMMAND_PREDICT
    virtual void commandPredict() = 0;
#endif
#if ENABLE_GUESS_GAME
    virtual void commandGuessGame() = 0;
#endif
#if ENABLE_COMMAND_OUTFIT
    virtual void commandChangeOutfit() = 0;
#endif
#if ENABLE_COMMAND_SPECIES
    virtual void commandChangeSpecies() = 0;
#endif
    virtual void commandStatus() = 0;
};

class CommandController
{
public:
    explicit CommandController(CommandHost &hostRef);

    void configure(const PetBehaviorConfig &config);
    void resetSelection();
    void next();
    void prev();
    bool executeCurrent();

    const char *currentLabel() const;
    AppCommandId currentCommandId() const;
    bool selectCommand(AppCommandId commandId);
    int commandCount() const;
    int selectedSlot() const;
    int previousSlot() const;
    bool isSlotVisible(int slot) const;

private:
    typedef bool (CommandController::*CanExecute)() const;
    typedef void (CommandController::*Execute)();

    struct CommandSlot
    {
        AppCommandId id;
        const char *label;
        CanExecute canExecute;
        Execute execute;
        bool visible;
        bool clearCommandAnimations;
    };

    CommandHost &host;
    int selected = 0;
    int previous = 0;
    CommandSlot slots[kPetBehaviorButtonCount] = {};

    static constexpr CommandSlot emptySlot();
    static CommandSlot systemCommandSlot(const char *token);
    static CommandSlot buttonSlot(const PetBehaviorButtonConfig &button);
    const CommandSlot &slotAt(int slot) const;

    bool canAlwaysExecute() const;
#if ENABLE_COMMAND_PREDICT
    bool canPredict() const;
#endif
    bool canStatus() const;
    bool hasAnimations(const AnimationId *ids, size_t count) const;

#if ENABLE_COMMAND_PREDICT
    void executePredict();
#endif
#if ENABLE_GUESS_GAME
    void executeGuessGame();
#endif
    void executeUserAction();
#if ENABLE_COMMAND_OUTFIT
    void executeChangeOutfit();
#endif
#if ENABLE_COMMAND_SPECIES
    void executeChangeSpecies();
#endif
    void executeStatus();
};

#endif // COMMAND_CONTROLLER_H
