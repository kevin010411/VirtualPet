#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H

#include <Arduino.h>
#include <stddef.h>
#include "shared/config/AppProfile.h"
#include "animation/domain/Animation.h"

enum class AppCommandId : uint8_t
{
    None = APP_COMMAND_NONE,
    FeedPet = APP_COMMAND_FEED_PET,
    Predict = APP_COMMAND_PREDICT,
    Gift = APP_COMMAND_GIFT,
    Medicine = APP_COMMAND_MEDICINE,
    Shower = APP_COMMAND_SHOWER,
    HaveFun = APP_COMMAND_HAVE_FUN,
    Clean = APP_COMMAND_CLEAN,
    ChangeOutfit = APP_COMMAND_CHANGE_OUTFIT,
    Status = APP_COMMAND_STATUS,
    ChangeSpecies = APP_COMMAND_CHANGE_SPECIES,
    Custom0,
    Custom1,
    Custom2,
    Custom3,
    Custom4,
    Custom5,
    Custom6,
    Custom7,
};

class CommandHost
{
public:
    virtual ~CommandHost() = default;

    virtual bool commandHasAnimation(AnimationId id) const = 0;
    virtual bool commandCanStatus() const = 0;
    virtual bool commandCanCustomAction(uint8_t slot) const = 0;
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
    virtual void commandChangeSpecies() = 0;
    virtual void commandStatus() = 0;
    virtual void commandCustomAction(uint8_t slot) = 0;
};

class CommandController
{
public:
    explicit CommandController(CommandHost &hostRef);

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
    bool canCustom0() const;
    bool canCustom1() const;
    bool canCustom2() const;
    bool canCustom3() const;
    bool canCustom4() const;
    bool canCustom5() const;
    bool canCustom6() const;
    bool canCustom7() const;
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
    void executeChangeSpecies();
    void executeStatus();
    void executeCustom0();
    void executeCustom1();
    void executeCustom2();
    void executeCustom3();
    void executeCustom4();
    void executeCustom5();
    void executeCustom6();
    void executeCustom7();
};

#endif // COMMAND_CONTROLLER_H
