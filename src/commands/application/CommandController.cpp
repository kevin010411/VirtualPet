#include "commands/application/CommandController.h"

#include <stddef.h>
#include "commands/domain/SystemCommandCatalog.h"

namespace
{
#define COMMAND_SLOT(commandId, label, canFn, execFn, clearFirst) \
    { commandId, label, &CommandController::canFn, &CommandController::execFn, true, clearFirst }
#if ENABLE_COMMAND_PREDICT
#define COMMAND_SLOT_PREDICT COMMAND_SLOT(AppCommandId::Predict, "PREDICT", canPredict, executePredict, true)
#else
#define COMMAND_SLOT_PREDICT CommandController::emptySlot()
#endif

#if ENABLE_COMMAND_OUTFIT
#define COMMAND_SLOT_CHANGE_OUTFIT COMMAND_SLOT(AppCommandId::ChangeOutfit, "CHANGE_OUTFIT", canAlwaysExecute, executeChangeOutfit, true)
#else
#define COMMAND_SLOT_CHANGE_OUTFIT CommandController::emptySlot()
#endif

#if ENABLE_COMMAND_SPECIES
#define COMMAND_SLOT_CHANGE_SPECIES COMMAND_SLOT(AppCommandId::ChangeSpecies, "CHANGE_SPECIES", canAlwaysExecute, executeChangeSpecies, true)
#else
#define COMMAND_SLOT_CHANGE_SPECIES CommandController::emptySlot()
#endif

#if ENABLE_GUESS_GAME
#define COMMAND_SLOT_GUESS_GAME COMMAND_SLOT(AppCommandId::HaveFun, "GUESS_GAME", canAlwaysExecute, executeGuessGame, false)
#else
#define COMMAND_SLOT_GUESS_GAME CommandController::emptySlot()
#endif
#define COMMAND_SLOT_STATUS COMMAND_SLOT(AppCommandId::Status, "STATUS", canStatus, executeStatus, true)
} // namespace

constexpr CommandController::CommandSlot CommandController::emptySlot()
{
    return {AppCommandId::None, "NO_OP", nullptr, nullptr, false, false};
}

CommandController::CommandSlot CommandController::systemCommandSlot(const char *token)
{
    const CompiledSystemCommand *command = findCompiledSystemCommand(token);
    if (command == nullptr)
        return emptySlot();

    switch (command->handler)
    {
#define SYSTEM_COMMAND(handler, token, slot) \
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
        return COMMAND_SLOT(AppCommandId::UserAction, "USER_ACTION", canAlwaysExecute, executeUserAction, false);
    if (button.kind == PetBehaviorButtonKind::SystemCommand)
        return systemCommandSlot(button.systemCommand);
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

    if (slot.clearCommandAnimations)
        host.commandClearCommandAnimations();

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
    const AnimationId required[] = {
        AnimationId::PredAnim,
        AnimationId::Predict1,
        AnimationId::Predict2,
        AnimationId::Predict3,
        AnimationId::Predict4,
        AnimationId::Predict5,
        AnimationId::Predict6,
        AnimationId::Predict7,
        AnimationId::Predict8,
        AnimationId::Predict9,
        AnimationId::Predict10,
        AnimationId::Predict11};
    return hasAnimations(required, sizeof(required) / sizeof(required[0]));
}
#endif

bool CommandController::canStatus() const
{
    return host.commandCanStatus();
}

bool CommandController::hasAnimations(const AnimationId *ids, size_t count) const
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
    host.commandHaveFun();
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
