#include "app/CommandController.h"

#include <stddef.h>

namespace
{
#define COMMAND_SLOT(commandId, label, canFn, execFn, clearFirst) \
    { commandId, label, &CommandController::canFn, &CommandController::execFn, true, clearFirst }

#if ENABLE_GUESS_ITEM_GAME
#define COMMAND_SLOT_HAVE_FUN COMMAND_SLOT(AppCommandId::HaveFun, "HAVE_FUN", canAlwaysExecute, executeHaveFun, false)
#else
#define COMMAND_SLOT_HAVE_FUN CommandController::emptySlot()
#endif
} // namespace

constexpr CommandController::CommandSlot CommandController::emptySlot()
{
    return {AppCommandId::None, "NO_OP", nullptr, nullptr, false, false};
}

const CommandController::CommandSlot CommandController::slots[] = {
#if APP_PROFILE == APP_PROFILE_NEW_TAIPEI_CHILDRENS_DAY
    COMMAND_SLOT(AppCommandId::FeedPet, "FEED_PET", canFeedPet, executeFeedPet, true),
    CommandController::emptySlot(),
    CommandController::emptySlot(),
    // COMMAND_SLOT(AppCommandId::ChangeSpecies, "CHANGE_SPECIES", canAlwaysExecute, executeChangeSpecies, true),
    COMMAND_SLOT(AppCommandId::Medicine, "MEDICINE", canMedicine, executeMedicine, true),
    COMMAND_SLOT(AppCommandId::Shower, "SHOWER", canShower, executeShower, true),
    COMMAND_SLOT(AppCommandId::Gift, "GIFT", canAlwaysExecute, executeGift, true),
    COMMAND_SLOT(AppCommandId::Clean, "CLEAN", canClean, executeClean, true),
    COMMAND_SLOT(AppCommandId::Status, "STATUS", canStatus, executeStatus, true),
#elif APP_PROFILE == APP_PROFILE_DEFAULT_SMALL
    COMMAND_SLOT(AppCommandId::FeedPet, "FEED_PET", canFeedPet, executeFeedPet, true),
    COMMAND_SLOT(AppCommandId::ChangeSpecies, "CHANGE_SPECIES", canAlwaysExecute, executeChangeSpecies, true),
    CommandController::emptySlot(),
    COMMAND_SLOT(AppCommandId::Medicine, "MEDICINE", canMedicine, executeMedicine, true),
    COMMAND_SLOT(AppCommandId::Shower, "SHOWER", canShower, executeShower, true),
    COMMAND_SLOT_HAVE_FUN,
    COMMAND_SLOT(AppCommandId::Clean, "CLEAN", canClean, executeClean, true),
    COMMAND_SLOT(AppCommandId::Status, "STATUS", canStatus, executeStatus, true),
#else
    COMMAND_SLOT(AppCommandId::FeedPet, "FEED_PET", canFeedPet, executeFeedPet, true),
    COMMAND_SLOT(AppCommandId::Predict, "PREDICT", canPredict, executePredict, true),
    COMMAND_SLOT(AppCommandId::Gift, "GIFT", canAlwaysExecute, executeGift, true),
    COMMAND_SLOT(AppCommandId::Medicine, "MEDICINE", canMedicine, executeMedicine, true),
    COMMAND_SLOT(AppCommandId::Shower, "SHOWER", canShower, executeShower, true),
    COMMAND_SLOT_HAVE_FUN,
    COMMAND_SLOT(AppCommandId::Clean, "CLEAN", canClean, executeClean, true),
    COMMAND_SLOT(AppCommandId::Status, "STATUS", canStatus, executeStatus, true),
#endif
};

CommandController::CommandController(CommandHost &hostRef)
    : host(hostRef)
{
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
    return sizeof(slots) / sizeof(slots[0]);
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

const CommandController::CommandSlot &CommandController::slotAt(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(sizeof(slots) / sizeof(slots[0])))
        return slots[0];

    return slots[slot];
}

bool CommandController::canAlwaysExecute() const
{
    return true;
}

bool CommandController::canFeedPet() const
{
    return host.commandHasAnimation(AnimationId::Feed);
}

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

bool CommandController::canMedicine() const
{
    return host.commandHasAnimation(AnimationId::Heal);
}

bool CommandController::canShower() const
{
    return host.commandHasAnimation(AnimationId::Shower);
}

bool CommandController::canClean() const
{
    return host.commandHasAnimation(AnimationId::Clean);
}

bool CommandController::canStatus() const
{
#if APP_STATUS_MODE == STATUS_MODE_STATUS
    return host.commandHasAnimation(AnimationId::Status);
#elif APP_STATUS_MODE == STATUS_MODE_RANDOM3
    return host.commandHasAnimation(AnimationId::StatusAge) ||
           host.commandHasAnimation(AnimationId::StatusHappy) ||
           host.commandHasAnimation(AnimationId::StatusHungry);
#else
    return host.commandHasAnimation(host.commandCurrentAgeAnimation());
#endif
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

void CommandController::executeFeedPet()
{
    host.commandFeedPet();
}

void CommandController::executePredict()
{
    host.commandPredict();
}

void CommandController::executeGift()
{
    host.commandGift();
}

void CommandController::executeMedicine()
{
    host.commandMedicine();
}

void CommandController::executeShower()
{
    host.commandShower();
}

#if ENABLE_GUESS_ITEM_GAME
void CommandController::executeHaveFun()
{
    host.commandHaveFun();
}
#endif

void CommandController::executeClean()
{
    host.commandClean();
}

void CommandController::executeChangeOutfit()
{
    host.commandChangeOutfit();
}

void CommandController::executeChangeSpecies()
{
    host.commandChangeSpecies();
}

void CommandController::executeStatus()
{
    host.commandStatus();
}
