#include "app/CommandExecutor.h"

CommandExecutor::CommandExecutor(PetActionController &petActionsRef, AnimationController &animationsRef)
    : petActions(petActionsRef),
      animations(animationsRef)
{
}

void CommandExecutor::begin(AppCommandId commandId)
{
    currentResult = {};
    currentResult.commandId = commandId;
}

CommandResult CommandExecutor::complete(bool executed)
{
    currentResult.executed = executed;
    return currentResult;
}

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

void CommandExecutor::queuePostCommandHappyAnimation()
{
    if (!animations.hasAnimation(AnimationId::Happy))
        return;

    animations.queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
}

void CommandExecutor::queueGiftAnimation()
{
    petActions.changeMood(50);
    animations.queueAnimation(Animation(AnimationId::Gift, gameTick * 2.5, false, AnimationOwner::Command, AnimationPriority::High));

    if (animations.hasAnimation(AnimationId::GiftHappy))
        animations.queueAnimation(Animation(AnimationId::GiftHappy, gameTick * 1.5, false, AnimationOwner::Command, AnimationPriority::High));

    queuePostCommandHappyAnimation();
    animations.markDirty();
}

bool CommandExecutor::commandHasAnimation(AnimationId id) const
{
    return animations.hasAnimation(id);
}

AnimationId CommandExecutor::commandCurrentAgeAnimation() const
{
    return petActions.currentAgeAnimation();
}

void CommandExecutor::commandClearCommandAnimations()
{
    animations.clearByOwner(AnimationOwner::Command);
}

void CommandExecutor::commandFeedPet()
{
    petActions.feedPet(40);
    animations.queueAnimation(Animation(AnimationId::Feed, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

void CommandExecutor::commandPredict()
{
    animations.queueAnimation(Animation(AnimationId::PredAnim, gameTick * 20, true, AnimationOwner::Command, AnimationPriority::High));
    animations.queueAnimation(Animation(fortuneToAnimationId(random(1, maxFortune + 1)), gameTick * 2.4, false, AnimationOwner::Command, AnimationPriority::High));
    animations.markDirty();
}

void CommandExecutor::commandGift()
{
    queueGiftAnimation();
}

void CommandExecutor::commandMedicine()
{
    petActions.takeMedicine();
    animations.queueAnimation(Animation(AnimationId::Heal, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

void CommandExecutor::commandShower()
{
    petActions.takeShower(250);
    animations.queueAnimation(Animation(AnimationId::Shower, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
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
    petActions.cleanEnvironment(500);
    animations.queueAnimation(Animation(AnimationId::Clean, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

void CommandExecutor::commandChangeOutfit()
{
    currentResult.requestedOutfit = true;
}

void CommandExecutor::commandChangeSpecies()
{
    currentResult.requestedSpecies = true;
}

void CommandExecutor::commandStatus()
{
    queueStatusAnimation();
}

void CommandExecutor::queueStatusAnimation()
{
#if APP_STATUS_MODE == STATUS_MODE_STATUS
    queueStatusDirectAnimation();
#elif APP_STATUS_MODE == STATUS_MODE_RANDOM3
    if (!queueStatusRandom3Animation())
        queueStatusAgeAnimation();
#else
    queueStatusAgeAnimation();
#endif
}

bool CommandExecutor::queueStatusDirectAnimation()
{
    if (!animations.hasAnimation(AnimationId::Status))
        return false;

    animations.queueAnimation(Animation(AnimationId::Status, gameTick * 10, true, AnimationOwner::Command, AnimationPriority::Normal));
    animations.markDirty();
    return true;
}

bool CommandExecutor::queueStatusAgeAnimation()
{
    const AnimationId ageAnimation = petActions.currentAgeAnimation();
    const uint16_t maxFrame = animations.frameCountFor(ageAnimation);
    if (maxFrame == 0)
        return false;

    animations.queueAnimation(Animation(ageAnimation, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, petActions.currentAgeFrame(maxFrame)));
    animations.markDirty();
    return true;
}

bool CommandExecutor::queueStatusRandom3Animation()
{
    AnimationId candidates[3] = {
        AnimationId::StatusAge,
        AnimationId::StatusHappy,
        AnimationId::StatusHungry,
    };

    const uint8_t start = random(3);
    for (uint8_t attempts = 0; attempts < 3; ++attempts)
    {
        const uint8_t index = (start + attempts) % 3;
        const AnimationId chosen = candidates[index];
        if (!animations.hasAnimation(chosen))
            continue;

        const uint16_t maxFrame = animations.frameCountFor(chosen);
        if (maxFrame == 0)
            continue;

        uint16_t frame = 1;
        if (chosen == AnimationId::StatusAge)
            frame = petActions.currentAgeFrame(maxFrame);
        else if (chosen == AnimationId::StatusHappy)
            frame = petActions.currentMoodFrame(maxFrame);
        else if (chosen == AnimationId::StatusHungry)
            frame = petActions.currentHungerFrame(maxFrame);
        else
            continue;

        animations.queueAnimation(Animation(chosen, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, frame));
        animations.markDirty();
        return true;
    }

    return false;
}

bool CommandExecutor::canPlayGuessItemGame() const
{
#if ENABLE_GUESS_ITEM_GAME
    const AnimationId required[] = {
        AnimationId::GuessItem1,
        AnimationId::GuessItem2,
        AnimationId::GuessItem3,
        AnimationId::GuessItem4,
        AnimationId::GuessLL,
        AnimationId::GuessLR,
        AnimationId::GuessRL,
        AnimationId::GuessRR};
    return animations.hasAnimations(required, sizeof(required) / sizeof(required[0]));
#else
    return false;
#endif
}
