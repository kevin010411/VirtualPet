#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H

#include <Arduino.h>
#include <stddef.h>
#include "common/AppProfile.h"
#include "animation/Animation.h"
#include "pet/PetBehaviorTypes.h"

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

class CommandController
{
public:
    CommandController();

    void configure(const PetBehaviorConfig &config);
    void resetSelection();
    void next();
    void prev();
    uint8_t currentActionSlot() const;

    const char *currentLabel() const;
    AppCommandId currentCommandId() const;
    bool selectCommand(AppCommandId commandId);
    int commandCount() const;
    int selectedSlot() const;
    int previousSlot() const;
    bool isSlotVisible(int slot) const;

private:
    struct CommandSlot
    {
        AppCommandId id;
        const char *label;
        uint8_t actionSlot;
        bool visible;
    };

    int selected = 0;
    int previous = 0;
    CommandSlot slots[kPetBehaviorButtonCount] = {};

    static constexpr CommandSlot emptySlot();
    static CommandSlot systemCommandSlot(RuntimeSystemCommandId runtimeId);
    static CommandSlot buttonSlot(const PetBehaviorButtonConfig &button);
    const CommandSlot &slotAt(int slot) const;

};

#endif // COMMAND_CONTROLLER_H
