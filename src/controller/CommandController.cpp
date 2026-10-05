#include "controller/CommandController.h"

#include <stddef.h>
#include "controller/SystemCommandCatalog.h"

namespace
{
#define COMMAND_SLOT(commandId, label) \
    { commandId, label, 0, true }
#if ENABLE_COMMAND_PREDICT
#define COMMAND_SLOT_PREDICT COMMAND_SLOT(AppCommandId::Predict, "PREDICT")
#else
#define COMMAND_SLOT_PREDICT CommandController::emptySlot()
#endif

#if ENABLE_COMMAND_OUTFIT
#define COMMAND_SLOT_CHANGE_OUTFIT COMMAND_SLOT(AppCommandId::ChangeOutfit, "CHANGE_OUTFIT")
#else
#define COMMAND_SLOT_CHANGE_OUTFIT CommandController::emptySlot()
#endif

#if ENABLE_GUESS_GAME
#define COMMAND_SLOT_GUESS_GAME COMMAND_SLOT(AppCommandId::GuessGame, "GUESS_GAME")
#else
#define COMMAND_SLOT_GUESS_GAME CommandController::emptySlot()
#endif
#define COMMAND_SLOT_STATUS COMMAND_SLOT(AppCommandId::Status, "STATUS")
} // namespace

constexpr CommandController::CommandSlot CommandController::emptySlot()
{
    return {AppCommandId::None, "NO_OP", 0, false};
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
#include "controller/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
    }
    return emptySlot();
}

CommandController::CommandSlot CommandController::buttonSlot(const PetBehaviorButtonConfig &button)
{
    if (!button.active || button.kind == PetBehaviorButtonKind::Empty)
        return emptySlot();
    if (button.kind == PetBehaviorButtonKind::UserAction)
        return {AppCommandId::UserAction, "USER_ACTION", button.actionSlot, true};
    if (button.kind == PetBehaviorButtonKind::SystemCommand)
        return systemCommandSlot(button.systemCommandId);
    return emptySlot();
}

CommandController::CommandController()
{
    for (uint8_t slot = 0; slot < kPetBehaviorButtonCount; ++slot)
        slots[slot] = emptySlot();
}

void CommandController::configure(const PetBehaviorConfig &config)
{
    for (uint8_t slot = 0; slot < kPetBehaviorButtonCount; ++slot)
    {
        slots[slot] = buttonSlot(config.buttons[slot]);
        bool displayed = false;
        for (uint8_t index = 0; index < config.screenBlockCount; ++index)
        {
            const ScreenBlockConfig &block = config.screenBlocks[index];
            displayed |= block.kind == ScreenBlockKind::Button && block.source == slot + 1;
        }
        slots[slot].visible &= displayed;
    }
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

uint8_t CommandController::currentActionSlot() const
{
    return slotAt(selected).actionSlot;
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
    return slot >= 0 && slot < commandCount() && slotAt(slot).visible;
}

const CommandController::CommandSlot &CommandController::slotAt(int slot) const
{
    if (slot < 0 || slot >= commandCount())
        return slots[0];

    return slots[slot];
}
