#include "commands/application/CommandExecutor.h"

#include <string.h>
#include "commands/domain/StatusSetContract.h"
#include "commands/domain/StatusSetSelection.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"

namespace
{
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

#if ENABLE_GUESS_GAME
void CommandExecutor::commandGuessGame()
{
    currentResult.requestedMinigame = canPlayGuessItemGame();
    currentResult.executed = currentResult.requestedMinigame;
}
#endif
#if ENABLE_COMMAND_PREDICT
FirmwarePlaybackRole CommandExecutor::fortuneToPlaybackRole(int fortuneIndex)
{
    switch (fortuneIndex)
    {
    case 1:
        return FirmwarePlaybackRole::Predict1;
    case 2:
        return FirmwarePlaybackRole::Predict2;
    case 3:
        return FirmwarePlaybackRole::Predict3;
    case 4:
        return FirmwarePlaybackRole::Predict4;
    case 5:
        return FirmwarePlaybackRole::Predict5;
    case 6:
        return FirmwarePlaybackRole::Predict6;
    case 7:
        return FirmwarePlaybackRole::Predict7;
    case 8:
        return FirmwarePlaybackRole::Predict8;
    case 9:
        return FirmwarePlaybackRole::Predict9;
    case 10:
        return FirmwarePlaybackRole::Predict10;
    default:
        return FirmwarePlaybackRole::Predict11;
    }
}
#endif
bool CommandExecutor::commandHasAnimation(FirmwarePlaybackRole id) const
{
    return animations.hasActionAnimation(id);
}

bool CommandExecutor::commandCanStatus() const
{
    return true;
}

#if ENABLE_COMMAND_PREDICT
void CommandExecutor::commandPredict()
{
    currentResult.layoutPlaybackRole = FirmwarePlaybackRole::PredAnim;
    const Animation sequence[] = {
        Animation(FirmwarePlaybackRole::PredAnim, gameTick * 20, true),
        Animation(fortuneToPlaybackRole(random(1, maxFortune + 1)), gameTick * 2.4, false),
    };
    currentResult.executed = animations.replace(
                                 AnimationSequence(sequence, sizeof(sequence) / sizeof(sequence[0]))) ==
                             PlaybackResult::Accepted;
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
    currentResult.layoutPlaybackRole = FirmwarePlaybackRole::Status;
    queueStatusAnimation();
}

void CommandExecutor::queueStatusAnimation()
{
    if (!queueStatusSetsAnimation())
        currentResult.resourceError = true;
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
        if (!animations.hasAnimation(resolution.animation))
            return false;
        const Animation animation(
            resolution.animation,
            gameTick * 10,
            true,
            0,
            FirmwarePlaybackRole::Status);
        return animations.replace(AnimationSequence(&animation, 1)) ==
               PlaybackResult::Accepted;
    }

    if (animations.frameCountFor(resolution.animation) != resolution.requiredFrames)
        return false;

    const Animation animation(
        resolution.animation,
        gameTick * 4,
        false,
        resolution.frame,
        FirmwarePlaybackRole::Status);
    return animations.replace(AnimationSequence(&animation, 1)) ==
           PlaybackResult::Accepted;
}

#if ENABLE_GUESS_GAME
bool CommandExecutor::canPlayGuessItemGame() const
{
    const FirmwarePlaybackRole requiredResults[] = {
#if ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
        FirmwarePlaybackRole::GuessLL,
        FirmwarePlaybackRole::GuessRR,
        FirmwarePlaybackRole::GuessWin,
        FirmwarePlaybackRole::GuessLoss};
#else
        FirmwarePlaybackRole::GuessLL,
        FirmwarePlaybackRole::GuessLR,
        FirmwarePlaybackRole::GuessRL,
        FirmwarePlaybackRole::GuessRR};
#endif
    if (!animations.hasAnimations(requiredResults, sizeof(requiredResults) / sizeof(requiredResults[0])))
        return false;

    const FirmwarePlaybackRole itemPrompts[] = {
        FirmwarePlaybackRole::GuessItem1,
        FirmwarePlaybackRole::GuessItem2,
        FirmwarePlaybackRole::GuessItem3,
        FirmwarePlaybackRole::GuessItem4};
    return animations.hasAnimation(FirmwarePlaybackRole::GuessStart) ||
           animations.hasAnimations(itemPrompts, sizeof(itemPrompts) / sizeof(itemPrompts[0]));
}
#endif
