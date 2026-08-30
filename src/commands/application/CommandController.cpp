#include "commands/application/CommandController.h"

#include <stddef.h>
#include "commands/domain/SystemCommandCatalog.h"

namespace
{
#define COMMAND_SLOT(commandId, label, canFn, execFn) \
    { commandId, label, &CommandController::canFn, &CommandController::execFn, true }
#if ENABLE_COMMAND_PREDICT
#define COMMAND_SLOT_PREDICT COMMAND_SLOT(AppCommandId::Predict, "PREDICT", canPredict, executePredict)
#else
#define COMMAND_SLOT_PREDICT CommandController::emptySlot()
#endif

#if ENABLE_COMMAND_OUTFIT
#define COMMAND_SLOT_CHANGE_OUTFIT COMMAND_SLOT(AppCommandId::ChangeOutfit, "CHANGE_OUTFIT", canAlwaysExecute, executeChangeOutfit)
#else
#define COMMAND_SLOT_CHANGE_OUTFIT CommandController::emptySlot()
#endif

#if ENABLE_COMMAND_SPECIES
#define COMMAND_SLOT_CHANGE_SPECIES COMMAND_SLOT(AppCommandId::ChangeSpecies, "CHANGE_SPECIES", canAlwaysExecute, executeChangeSpecies)
#else
#define COMMAND_SLOT_CHANGE_SPECIES CommandController::emptySlot()
#endif

#if ENABLE_GUESS_GAME
#define COMMAND_SLOT_GUESS_GAME COMMAND_SLOT(AppCommandId::GuessGame, "GUESS_GAME", canAlwaysExecute, executeGuessGame)
#else
#define COMMAND_SLOT_GUESS_GAME CommandController::emptySlot()
#endif
#define COMMAND_SLOT_STATUS COMMAND_SLOT(AppCommandId::Status, "STATUS", canStatus, executeStatus)
} // namespace

constexpr CommandController::CommandSlot CommandController::emptySlot()
{
    return {AppCommandId::None, "NO_OP", nullptr, nullptr, false};
}

CommandController::CommandSlot CommandController::systemCommandSlot(RuntimeSystemCommandId runtimeId)
{
    const CompiledSystemCommand *command = findCompiledSystemCommand(runtimeId);
    if (command == nullptr)
        return emptySlot();

    switch (command->handler)
    {
#define SYSTEM_COMMAND(handler, token, runtimeId, slot) \
    case SystemCommandHandler::handler:      \
        return slot;
#include "commands/domain/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
    }
    return emptySlot();
}

CommandController::CommandSlot CommandController::buttonSlot(const PetBehaviorButtonConfig &button)
{
    if (!button.active || button.kind == PetBehaviorButtonKind::Empty)
        return emptySlot();
    if (button.kind == PetBehaviorButtonKind::UserAction)
        return COMMAND_SLOT(AppCommandId::UserAction, "USER_ACTION", canAlwaysExecute, executeUserAction);
    if (button.kind == PetBehaviorButtonKind::SystemCommand)
        return systemCommandSlot(button.systemCommandId);
    return emptySlot();
}

CommandController::CommandController(CommandHost &hostRef)
    : host(hostRef)
{
    for (uint8_t slot = 0; slot < kPetBehaviorButtonCount; ++slot)
        slots[slot] = emptySlot();
}

void CommandController::configure(const PetBehaviorConfig &config)
{
    for (uint8_t slot = 0; slot < kPetBehaviorButtonCount; ++slot)
        slots[slot] = buttonSlot(config.buttons[slot]);
}

void CommandController::resetSelection()
{
    selected = 0;
    previous = selected;
    if (!isSlotVisible(selected))
        next();
}

void CommandController::next()
{
    previous = selected;
    for (int step = 0; step < commandCount(); ++step)
    {
        selected = (selected + 1) % commandCount();
        if (isSlotVisible(selected))
            break;
    }
}

void CommandController::prev()
{
    previous = selected;
    for (int step = 0; step < commandCount(); ++step)
    {
        selected = (selected == 0) ? (commandCount() - 1) : (selected - 1);
        if (isSlotVisible(selected))
            break;
    }
}

bool CommandController::executeCurrent()
{
    const CommandSlot &slot = slotAt(selected);
    if (!slot.visible || slot.execute == nullptr)
        return false;

    if (slot.canExecute != nullptr && !(this->*slot.canExecute)())
        return false;

    (this->*slot.execute)();
    return true;
}

const char *CommandController::currentLabel() const
{
    return slotAt(selected).label;
}

AppCommandId CommandController::currentCommandId() const
{
    return slotAt(selected).id;
}

bool CommandController::selectCommand(AppCommandId commandId)
{
    for (int i = 0; i < commandCount(); ++i)
    {
        const CommandSlot &slot = slotAt(i);
        if (slot.visible && slot.id == commandId)
        {
            previous = selected;
            selected = i;
            return true;
        }
    }

    return false;
}

int CommandController::commandCount() const
{
    return kPetBehaviorButtonCount;
}

int CommandController::selectedSlot() const
{
    return selected;
}

int CommandController::previousSlot() const
{
    return previous;
}

bool CommandController::isSlotVisible(int slot) const
{
    return slotAt(slot).visible;
}

const CommandController::CommandSlot &CommandController::slotAt(int slot) const
{
    if (slot < 0 || slot >= commandCount())
        return slots[0];

    return slots[slot];
}

bool CommandController::canAlwaysExecute() const
{
    return true;
}

#if ENABLE_COMMAND_PREDICT
bool CommandController::canPredict() const
{
    const FirmwarePlaybackRole required[] = {
        FirmwarePlaybackRole::PredAnim,
        FirmwarePlaybackRole::Predict1,
        FirmwarePlaybackRole::Predict2,
        FirmwarePlaybackRole::Predict3,
        FirmwarePlaybackRole::Predict4,
        FirmwarePlaybackRole::Predict5,
        FirmwarePlaybackRole::Predict6,
        FirmwarePlaybackRole::Predict7,
        FirmwarePlaybackRole::Predict8,
        FirmwarePlaybackRole::Predict9,
        FirmwarePlaybackRole::Predict10,
        FirmwarePlaybackRole::Predict11};
    return hasAnimations(required, sizeof(required) / sizeof(required[0]));
}
#endif

bool CommandController::canStatus() const
{
    return host.commandCanStatus();
}

bool CommandController::hasAnimations(const FirmwarePlaybackRole *ids, size_t count) const
{
    if (ids == nullptr)
        return false;

    for (size_t i = 0; i < count; ++i)
    {
        if (!host.commandHasAnimation(ids[i]))
            return false;
    }
    return true;
}

#if ENABLE_COMMAND_PREDICT
void CommandController::executePredict()
{
    host.commandPredict();
}
#endif

#if ENABLE_GUESS_GAME
void CommandController::executeGuessGame()
{
    host.commandGuessGame();
}
#endif

void CommandController::executeUserAction()
{
    // Game intercepts configured user-action slots before legacy execution.
}

#if ENABLE_COMMAND_OUTFIT
void CommandController::executeChangeOutfit()
{
    host.commandChangeOutfit();
}
#endif

#if ENABLE_COMMAND_SPECIES
void CommandController::executeChangeSpecies()
{
    host.commandChangeSpecies();
}
#endif

void CommandController::executeStatus()
{
    host.commandStatus();
}
