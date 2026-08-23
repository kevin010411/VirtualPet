#include "commands/application/CommandExecutor.h"

#include <string.h>
#include "commands/domain/StatusSetContract.h"
#include "commands/domain/StatusSetSelection.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"
#include "shared/assets/AssetManifest.h"

namespace
{
static_assert(
    AssetManifest::kMaxAnimationNameLength + 1 >= kStatusAnimationNameSize,
    "Asset manifest names must hold every Status Animation ID.");
static_assert(
    sizeof(((Animation *)nullptr)->namedAnimation) >= kStatusAnimationNameSize,
    "Queued animation names must hold every Status Animation ID.");
#if !ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
uint8_t arduinoStatusSetIndex(uint8_t setCount)
{
    return static_cast<uint8_t>(random(setCount));
}
#endif

struct StatusValueContext
{
    const PetStatSnapshot &stats;
    const PetBehaviorConfig &config;
};

bool petStatSlotForSource(
    const PetBehaviorConfig &config,
    const char *source,
    uint8_t &slot)
{
    return ActivePetBehaviorStatSlots(config).resolve(source, slot);
}

bool statusValueFromSnapshot(const char *source, const void *context, int32_t &value)
{
    if (source == nullptr || context == nullptr)
        return false;
    const StatusValueContext &status = *static_cast<const StatusValueContext *>(context);
    if (strcmp(source, "stage_days") == 0)
    {
        value = static_cast<int32_t>(status.stats.stage_days);
        return true;
    }
    uint8_t slot = 0;
    if (!petStatSlotForSource(status.config, source, slot))
        return false;
    value = status.stats.customStats[slot];
    return true;
}
} // namespace

CommandExecutor::CommandExecutor(PetActionController &petActionsRef, AnimationController &animationsRef)
    : petActions(petActionsRef),
      animations(animationsRef)
{
}

void CommandExecutor::begin(AppCommandId commandId)
{
    currentResult = {};
    currentResult.commandId = commandId;
    currentResult.executed = true;
}

CommandResult CommandExecutor::complete(bool executed)
{
    currentResult.executed = executed && currentResult.executed;
    return currentResult;
}

void CommandExecutor::configureRuntimeContract(const PetBehaviorConfig &config)
{
    petBehaviorConfig = &config;
}

#if ENABLE_COMMAND_PREDICT
AnimationId CommandExecutor::fortuneToAnimationId(int fortuneIndex)
{
    switch (fortuneIndex)
    {
    case 1:
        return AnimationId::Predict1;
    case 2:
        return AnimationId::Predict2;
    case 3:
        return AnimationId::Predict3;
    case 4:
        return AnimationId::Predict4;
    case 5:
        return AnimationId::Predict5;
    case 6:
        return AnimationId::Predict6;
    case 7:
        return AnimationId::Predict7;
    case 8:
        return AnimationId::Predict8;
    case 9:
        return AnimationId::Predict9;
    case 10:
        return AnimationId::Predict10;
    default:
        return AnimationId::Predict11;
    }
}
#endif
bool CommandExecutor::commandHasAnimation(AnimationId id) const
{
    return animations.hasActionAnimation(id);
}

bool CommandExecutor::commandCanStatus() const
{
    return true;
}

void CommandExecutor::commandClearCommandAnimations()
{
    animations.clearByOwner(AnimationOwner::Command);
}

#if ENABLE_COMMAND_PREDICT
void CommandExecutor::commandPredict()
{
    currentResult.layoutId = AnimationId::PredAnim;
    animations.queueAnimation(Animation(AnimationId::PredAnim, gameTick * 20, true, AnimationOwner::Command, AnimationPriority::High));
    animations.queueAnimation(Animation(fortuneToAnimationId(random(1, maxFortune + 1)), gameTick * 2.4, false, AnimationOwner::Command, AnimationPriority::High));
    animations.markDirty();
}
#endif

#if ENABLE_GUESS_GAME
void CommandExecutor::commandHaveFun()
{
    animations.clearByOwner(AnimationOwner::Command);
    currentResult.requestedMinigame = canPlayGuessItemGame();
    currentResult.executed = currentResult.requestedMinigame;
}
#endif

#if ENABLE_COMMAND_OUTFIT
void CommandExecutor::commandChangeOutfit()
{
    currentResult.requestedOutfit = true;
}
#endif

#if ENABLE_COMMAND_SPECIES
void CommandExecutor::commandChangeSpecies()
{
    currentResult.requestedSpecies = true;
}
#endif

void CommandExecutor::commandStatus()
{
    currentResult.layoutId = AnimationId::Status;
    queueStatusAnimation();
}

void CommandExecutor::queueStatusAnimation()
{
    if (!queueStatusSetsAnimation())
        showStatusNotFound();
}

void CommandExecutor::showStatusNotFound()
{
    animations.showStatusNotFound();
}

bool CommandExecutor::queueStatusSetsAnimation()
{
    if (petBehaviorConfig == nullptr)
        return false;
    uint8_t selectedSetIndex = 0;
#if ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
    const uint8_t setCount = petBehaviorConfig->statusSets.count;
    if (setCount == 0)
        return false;
    selectedSetIndex = static_cast<uint8_t>(nextStatusSetIndex % setCount);
    nextStatusSetIndex = static_cast<uint8_t>((selectedSetIndex + 1) % setCount);
#else
    if (!selectStatusSetIndex(petBehaviorConfig->statusSets.count, arduinoStatusSetIndex, selectedSetIndex))
        return false;
#endif
    const StatusSetConfig &set = petBehaviorConfig->statusSets.sets[selectedSetIndex];
    const PetStatSnapshot stats = petActions.statSnapshot();
    const StatusValueContext valueContext = {stats, *petBehaviorConfig};
    StatusSetResolution resolution = {};
    if (!resolveStatusSet(set, statusValueFromSnapshot, &valueContext, resolution))
        return false;

    if (resolution.playOnce)
    {
        if (!animations.hasAnimation(AnimationId::Status))
            return false;
        animations.queueAnimation(Animation(
            AnimationId::Status,
            gameTick * 10,
            true,
            AnimationOwner::Command,
            AnimationPriority::Normal));
        animations.markDirty();
        return true;
    }

    if (animations.frameCountForName(resolution.animation) != resolution.requiredFrames)
        return false;

    animations.queueNamedAnimation(
        resolution.animation,
        gameTick * 4,
        false,
        AnimationOwner::Command,
        AnimationPriority::Normal,
        resolution.frame);
    animations.markDirty();
    return true;
}

#if ENABLE_GUESS_GAME
bool CommandExecutor::canPlayGuessItemGame() const
{
    const AnimationId requiredResults[] = {
#if ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
        AnimationId::GuessLL,
        AnimationId::GuessRR,
        AnimationId::GuessWin,
        AnimationId::GuessLoss};
#else
        AnimationId::GuessLL,
        AnimationId::GuessLR,
        AnimationId::GuessRL,
        AnimationId::GuessRR};
#endif
    if (!animations.hasAnimations(requiredResults, sizeof(requiredResults) / sizeof(requiredResults[0])))
        return false;

    const AnimationId itemPrompts[] = {
        AnimationId::GuessItem1,
        AnimationId::GuessItem2,
        AnimationId::GuessItem3,
        AnimationId::GuessItem4};
    return animations.hasAnimation(AnimationId::GuessStart) ||
           animations.hasAnimations(itemPrompts, sizeof(itemPrompts) / sizeof(itemPrompts[0]));
}
#endif
