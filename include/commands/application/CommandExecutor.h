#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include <Arduino.h>
#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "pet/application/PetActionController.h"

class CustomRules;
struct PetBehaviorConfig;

struct CommandResult
{
    bool executed = false;
    AppCommandId commandId = AppCommandId::None;
    AnimationId layoutId = AnimationId::None;
    bool requestedOutfit = false;
    bool requestedSpecies = false;
    bool requestedMinigame = false;
};

class CommandExecutor : public CommandHost
{
public:
    CommandExecutor(PetActionController &petActions, AnimationController &animations, CustomRules &customRules);

    void begin(AppCommandId commandId);
    CommandResult complete(bool executed);
    bool validateRequiredContracts(const PetBehaviorConfig &config) const;

private:
    static constexpr unsigned long gameTick = 2000;
    static constexpr int maxFortune = 11;

    PetActionController &petActions;
    AnimationController &animations;
    CustomRules &customRules;
    CommandResult currentResult = {};

#if ENABLE_CUSTOM_RULES
    bool executeCustomAction(const char *actionKey);
#endif
#if ENABLE_COMMAND_PREDICT
    static AnimationId fortuneToAnimationId(int fortuneIndex);
#endif
    bool queueCommandAction(AnimationId id,
                            unsigned long durationMs,
                            bool playOnce = false,
                            char *selectedName = nullptr,
                            size_t selectedNameSize = 0);
#if ENABLE_COMMAND_GIFT || ENABLE_GUESS_ITEM_GAME
    void queueGiftAnimation();
#endif
    void queueStatusAnimation();
    void showStatusNotFound();
    bool queueStatusSetsAnimation();
#if ENABLE_GUESS_ITEM_GAME
    bool canPlayGuessItemGame() const;
#endif

    bool commandHasAnimation(AnimationId id) const override;
    bool commandCanStatus() const override;
#if ENABLE_CUSTOM_RULES
    bool commandCanCustomAction(uint8_t slot) const;
#endif
    void commandClearCommandAnimations() override;
    void commandFeedPet();
#if ENABLE_COMMAND_PREDICT
    void commandPredict() override;
#endif
#if ENABLE_COMMAND_GIFT
    void commandGift();
#endif
    void commandMedicine();
    void commandShower();
#if ENABLE_GUESS_ITEM_GAME
    void commandHaveFun() override;
#endif
    void commandClean();
#if ENABLE_COMMAND_OUTFIT
    void commandChangeOutfit() override;
#endif
#if ENABLE_COMMAND_SPECIES
    void commandChangeSpecies() override;
#endif
    void commandStatus() override;
#if ENABLE_CUSTOM_RULES
    void commandCustomAction(uint8_t slot);
#endif
};

#endif // COMMAND_EXECUTOR_H
