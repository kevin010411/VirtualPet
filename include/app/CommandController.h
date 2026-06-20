#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H

#include <Arduino.h>
#include <stddef.h>
#include "app/AppProfile.h"
#include "domain/Animation.h"

class CommandHost
{
public:
    virtual ~CommandHost() = default;

    virtual bool commandHasAnimation(AnimationId id) const = 0;
    virtual AnimationId commandCurrentAgeAnimation() const = 0;
    virtual void commandClearCommandAnimations() = 0;

    virtual void commandFeedPet() = 0;
    virtual void commandPredict() = 0;
    virtual void commandGift() = 0;
    virtual void commandMedicine() = 0;
    virtual void commandShower() = 0;
#if ENABLE_GUESS_ITEM_GAME
    virtual void commandHaveFun() = 0;
#endif
    virtual void commandClean() = 0;
    virtual void commandChangeOutfit() = 0;
    virtual void commandStatus() = 0;
};

class CommandController
{
public:
    explicit CommandController(CommandHost &hostRef);

    void resetSelection();
    void next();
    void prev();
    void executeCurrent();

    const char *currentLabel() const;
    int commandCount() const;
    int selectedSlot() const;
    int previousSlot() const;
    bool isSlotVisible(int slot) const;

private:
    typedef bool (CommandController::*CanExecute)() const;
    typedef void (CommandController::*Execute)();

    struct CommandSlot
    {
        const char *label;
        CanExecute canExecute;
        Execute execute;
        bool visible;
        bool clearCommandAnimations;
    };

    CommandHost &host;
    int selected = 0;
    int previous = 0;

    static constexpr CommandSlot emptySlot();
    static const CommandSlot slots[];
    static const CommandSlot &slotAt(int slot);

    bool canAlwaysExecute() const;
    bool canFeedPet() const;
    bool canPredict() const;
    bool canMedicine() const;
    bool canShower() const;
    bool canClean() const;
    bool canStatus() const;
    bool hasAnimations(const AnimationId *ids, size_t count) const;

    void executeFeedPet();
    void executePredict();
    void executeGift();
    void executeMedicine();
    void executeShower();
#if ENABLE_GUESS_ITEM_GAME
    void executeHaveFun();
#endif
    void executeClean();
    void executeChangeOutfit();
    void executeStatus();
};

#endif // COMMAND_CONTROLLER_H
