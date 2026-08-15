#include "commands/application/CommandExecutor.h"

#include <string.h>
#include "commands/domain/StatusSetContract.h"
#include "commands/domain/StatusSetSelection.h"
#include "custom_rules/domain/CustomRules.h"
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
static_assert(
    AssetManifest::kMaxNamedAnimations >= kMaxStatusSets,
    "The manifest must hold all configured Status Sets.");

constexpr const char *kStatusSetsPath = "/status_sets.txt";

uint8_t arduinoStatusSetIndex(uint8_t setCount)
{
    return static_cast<uint8_t>(random(setCount));
}

bool loadStatusSetsConfig(SdFat *sd, StatusSetsConfig &config)
{
    config = {};
    if (sd == nullptr)
        return false;
    File file = sd->open(kStatusSetsPath, FILE_READ);
    if (!file)
        return false;
    char contract[kMaxStatusContractBytes] = {};
    size_t index = 0;
    while (file.available())
    {
        if (index + 1 >= sizeof(contract))
        {
            file.close();
            return false;
        }
        contract[index++] = static_cast<char>(file.read());
    }
    file.close();
    contract[index] = '\0';
    return parseStatusSetsContract(contract, config);
}

bool statusValueFromSnapshot(const char *source, const void *context, int32_t &value)
{
    if (source == nullptr || context == nullptr)
        return false;
    const PetStatSnapshot &stats = *static_cast<const PetStatSnapshot *>(context);
    if (strcmp(source, "age") == 0)
        value = stats.age;
    else if (strcmp(source, "hunger") == 0)
        value = stats.hunger;
    else if (strcmp(source, "mood") == 0)
        value = stats.mood;
    else if (strcmp(source, "clean") == 0)
        value = stats.clean;
    else if (strcmp(source, "env") == 0)
        value = stats.env;
    else if (strcmp(source, "sick") == 0)
        value = stats.sick;
    else if (strcmp(source, "stage_days") == 0)
        value = static_cast<int32_t>(stats.stage_days);
    else if (strcmp(source, "health") == 0)
        value = stats.health;
    else
    {
        if (strncmp(source, "custom", 6) != 0 ||
            source[6] < '0' || source[6] > '7' || source[7] != '\0')
            return false;
        value = stats.customStats[source[6] - '0'];
    }
    return true;
}
} // namespace

CommandExecutor::CommandExecutor(PetActionController &petActionsRef, AnimationController &animationsRef, CustomRules &customRulesRef)
    : petActions(petActionsRef),
      animations(animationsRef),
      customRules(customRulesRef)
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

bool CommandExecutor::validateRequiredContracts(const PetBehaviorConfig &config) const
{
    bool statusRequired = false;
    for (uint8_t index = 0; index < config.buttonCount; ++index)
    {
        const PetBehaviorButtonConfig &button = config.buttons[index];
        if (button.active && button.kind == PetBehaviorButtonKind::SystemCommand &&
            strcmp(button.systemCommand, "status") == 0)
        {
            statusRequired = true;
            break;
        }
    }
    if (!statusRequired)
        return true;

    StatusSetsConfig statusConfig = {};
    if (!loadStatusSetsConfig(animations.sdCard(), statusConfig))
        return false;
    for (uint8_t setIndex = 0; setIndex < statusConfig.count; ++setIndex)
    {
        const StatusSetConfig &set = statusConfig.sets[setIndex];
        for (uint8_t conditionIndex = 0; conditionIndex < set.conditionCount; ++conditionIndex)
        {
            const char *source = set.conditions[conditionIndex].source;
            if (strncmp(source, "custom", 6) != 0)
                continue;
            const uint8_t statSlot = static_cast<uint8_t>(source[6] - '0');
            if (statSlot >= kPetBehaviorSlotCount || !config.stats[statSlot].active)
                return false;
        }
    }
    return true;
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

bool CommandExecutor::queueCommandAction(AnimationId id,
                                         unsigned long durationMs,
                                         bool playOnce,
                                         char *selectedName,
                                         size_t selectedNameSize)
{
    const bool queued = animations.queueActionAnimation(
        id, durationMs, playOnce, AnimationOwner::Command, AnimationPriority::High, selectedName, selectedNameSize);
    if (!queued)
        animations.showResourceError();
    return queued;
}

#if ENABLE_COMMAND_GIFT || ENABLE_GUESS_ITEM_GAME
void CommandExecutor::queueGiftAnimation()
{
    char selectedAnimation[16] = {};
    const bool queued = queueCommandAction(AnimationId::Gift, gameTick * 2.5, false, selectedAnimation, sizeof(selectedAnimation));
    if (!queued || !customRules.applyVariantEffect(selectedAnimation, petActions))
        petActions.changeMood(50);

    if (animations.hasActionAnimation(AnimationId::GiftHappy))
        animations.queueActionAnimation(AnimationId::GiftHappy, gameTick * 1.5, false, AnimationOwner::Command, AnimationPriority::High);

    animations.markDirty();
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

#if ENABLE_CUSTOM_RULES
bool CommandExecutor::commandCanCustomAction(uint8_t slot) const
{
    if (slot > 7)
        return false;
    const char key[] = {'C', 'U', 'S', 'T', 'O', 'M', static_cast<char>('0' + slot), '\0'};
    return customRules.hasAction(key);
}
#endif

void CommandExecutor::commandClearCommandAnimations()
{
    animations.clearByOwner(AnimationOwner::Command);
}

void CommandExecutor::commandFeedPet()
{
    currentResult.layoutId = AnimationId::Feed;
    petActions.feedPet(40);
    queueCommandAction(AnimationId::Feed, gameTick * 1.2);
    animations.markDirty();
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

#if ENABLE_COMMAND_GIFT
void CommandExecutor::commandGift()
{
    currentResult.layoutId = AnimationId::Gift;
    queueGiftAnimation();
}
#endif

void CommandExecutor::commandMedicine()
{
    currentResult.layoutId = AnimationId::Heal;
    petActions.takeMedicine();
    queueCommandAction(AnimationId::Heal, gameTick * 1.2);
    animations.markDirty();
}

void CommandExecutor::commandShower()
{
    currentResult.layoutId = AnimationId::Shower;
    petActions.takeShower(250);
    queueCommandAction(AnimationId::Shower, gameTick * 1.2);
    animations.markDirty();
}

#if ENABLE_GUESS_ITEM_GAME
void CommandExecutor::commandHaveFun()
{
    animations.clearByOwner(AnimationOwner::Command);
    if (canPlayGuessItemGame())
        currentResult.requestedMinigame = true;
    else
        queueGiftAnimation();
}
#endif

void CommandExecutor::commandClean()
{
    currentResult.layoutId = AnimationId::Clean;
    petActions.cleanEnvironment(500);
    queueCommandAction(AnimationId::Clean, gameTick * 1.2);
    animations.markDirty();
}

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

#if ENABLE_CUSTOM_RULES
void CommandExecutor::commandCustomAction(uint8_t slot)
{
    if (slot > 7)
    {
        currentResult.executed = false;
        return;
    }
    const char key[] = {'C', 'U', 'S', 'T', 'O', 'M', static_cast<char>('0' + slot), '\0'};
    currentResult.executed = executeCustomAction(key);
}
#endif

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
    StatusSetsConfig config = {};
    if (!loadStatusSetsConfig(animations.sdCard(), config))
        return false;

    uint8_t selectedSetIndex = 0;
    if (!selectStatusSetIndex(config.count, arduinoStatusSetIndex, selectedSetIndex))
        return false;
    const StatusSetConfig &set = config.sets[selectedSetIndex];
    const PetStatSnapshot stats = petActions.statSnapshot();
    StatusSetResolution resolution = {};
    if (!resolveStatusSet(set, statusValueFromSnapshot, &stats, resolution))
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

#if ENABLE_GUESS_ITEM_GAME
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

#if ENABLE_CUSTOM_RULES
bool CommandExecutor::executeCustomAction(const char *actionKey)
{
    return customRules.executeAction(actionKey, petActions, animations);
}
#endif
