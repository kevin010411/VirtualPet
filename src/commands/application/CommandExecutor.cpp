#include "commands/application/CommandExecutor.h"

#include "commands/domain/StatusSetContract.h"
#include "commands/domain/StatusSetSelection.h"
#include "pet_behavior/application/PetBehaviorRuntime.h"
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
    const ActivePetState &activePetState;
};

bool statusValueFromSnapshot(const StatusSetCondition &condition,
                             const void *context,
                             int32_t &value)
{
    if (context == nullptr)
        return false;
    const StatusValueContext &status = *static_cast<const StatusValueContext *>(context);
    if (condition.source == StatusConditionSource::PetStatus)
        return resolvePetStateStatusLevel(condition, status.activePetState, value);
    if (condition.source != StatusConditionSource::PetStat &&
        condition.source != StatusConditionSource::StageDays)
        return false;
    RuntimeValueContext context = {};
    context.petStats = status.stats.customStats;
    context.petStatCapacity = PetStatSnapshot::kCustomStatCount;
    context.behaviorConfig = &status.config;
    context.stageDays = status.stats.stage_days;
    return resolveRuntimeValue(condition.valueId, context, value);
}
} // namespace

CommandExecutor::CommandExecutor(
    PetActionController &petActionsRef,
    AnimationController &animationsRef,
    const PetBehaviorRuntime &petBehaviorRuntimeRef)
    : petActions(petActionsRef),
      animations(animationsRef),
      petBehaviorRuntime(petBehaviorRuntimeRef)
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
    const ActivePetState activePetState = petBehaviorRuntime.activePetState(stats);
    const StatusValueContext valueContext = {
        stats, *petBehaviorConfig, activePetState};
    StatusSetResolution resolution = {};
    if (!resolveStatusSet(set, statusValueFromSnapshot, &valueContext, resolution))
        return false;

    if (!resolution.playOnce ||
        animations.versionCountFor(resolution.animation) != resolution.requiredVersions)
        return false;
    Animation animation = Animation::complete(
        resolution.animation, 1, FirmwarePlaybackRole::Status);
    animation.versionIndex = resolution.versionIndex;
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
