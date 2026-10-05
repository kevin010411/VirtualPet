#include "controller/CommandExecutor.h"

#include "pet/Pet.h"
#include "controller/StatusSetContract.h"
#include "controller/StatusSetSelection.h"
#include "pet/PetBehaviorRuntime.h"
#include "pet/PetBehaviorStatSlot.h"
#include "pet/PetBehaviorTypes.h"
#include "common/FirmwareRandom.h"

namespace
{
#if !ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
uint8_t firmwareStatusSetIndex(uint8_t setCount)
{
    return static_cast<uint8_t>(FirmwareRandom::below(setCount));
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
    if (condition.kind == StatusConditionKind::PetStatusAxis)
        return resolvePetStateStatusLevel(condition, status.activePetState, value);
    if (condition.kind != StatusConditionKind::RuntimeValue)
        return false;
    RuntimeValueContext runtimeValueContext = {};
    runtimeValueContext.petStats = status.stats.customStats;
    runtimeValueContext.activePetStatMask = activePetBehaviorStatMask(status.config);
    runtimeValueContext.stageDays = status.stats.stage_days;
    return resolveRuntimeValue(condition.valueId, runtimeValueContext, value);
}
} // namespace

CommandExecutor::CommandExecutor(
    const Pet &petRef,
    AnimationController &animationsRef,
    PetBehaviorRuntime &petBehaviorRuntimeRef)
    : pet(petRef),
      animations(animationsRef),
      petBehaviorRuntime(petBehaviorRuntimeRef)
{
}

CommandResult CommandExecutor::execute(const CommandController &commands)
{
    CommandResult result = {};
    result.commandId = commands.currentCommandId();
    if (!commands.isSlotVisible(commands.selectedSlot()))
        return result;

    switch (result.commandId)
    {
    case AppCommandId::UserAction:
        result.actionResult = petBehaviorRuntime.executeAction(commands.currentActionSlot());
        result.executed = result.actionResult != PetBehaviorActionResult::Rejected;
        result.resourceError = result.actionResult == PetBehaviorActionResult::AppliedAnimationMissing;
        break;
    case AppCommandId::Status:
        result.executed = true;
        result.resourceError = !queueStatusSetsAnimation();
        break;
#if ENABLE_COMMAND_PREDICT
    case AppCommandId::Predict:
        if (canPredict())
            commandPredict(result);
        break;
#endif
#if ENABLE_GUESS_GAME
    case AppCommandId::GuessGame:
        result.requestedMinigame = canPlayGuessItemGame();
        result.executed = result.requestedMinigame;
        break;
#endif
#if ENABLE_COMMAND_OUTFIT
    case AppCommandId::ChangeOutfit:
        result.requestedOutfit = true;
        result.executed = true;
        break;
#endif
    default:
        break;
    }
    return result;
}

void CommandExecutor::configureRuntimeContract(const PetBehaviorConfig &config)
{
    petBehaviorConfig = &config;
}

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
#if ENABLE_COMMAND_PREDICT
void CommandExecutor::commandPredict(CommandResult &result)
{
    const Animation sequence[] = {
        Animation(FirmwarePlaybackRole::PredAnim, gameTick * 20, true),
        Animation(fortuneToPlaybackRole(1 + FirmwareRandom::below(maxFortune)), gameTick * 2.4, false),
    };
    result.executed = animations.replace(
                                 AnimationSequence(sequence, sizeof(sequence) / sizeof(sequence[0]))) ==
                             PlaybackResult::Accepted;
}
#endif

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
    if (!selectStatusSetIndex(petBehaviorConfig->statusSets.count, firmwareStatusSetIndex, selectedSetIndex))
        return false;
#endif
    const StatusSetConfig &set = petBehaviorConfig->statusSets.sets[selectedSetIndex];
    const PetStatSnapshot stats = pet.statSnapshot();
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

#if ENABLE_COMMAND_PREDICT
bool CommandExecutor::canPredict() const
{
    const FirmwarePlaybackRole required[] = {
        FirmwarePlaybackRole::PredAnim,
        FirmwarePlaybackRole::Predict1, FirmwarePlaybackRole::Predict2,
        FirmwarePlaybackRole::Predict3, FirmwarePlaybackRole::Predict4,
        FirmwarePlaybackRole::Predict5, FirmwarePlaybackRole::Predict6,
        FirmwarePlaybackRole::Predict7, FirmwarePlaybackRole::Predict8,
        FirmwarePlaybackRole::Predict9, FirmwarePlaybackRole::Predict10,
        FirmwarePlaybackRole::Predict11};
    for (FirmwarePlaybackRole role : required)
        if (!animations.hasActionAnimation(role))
            return false;
    return true;
}
#endif
