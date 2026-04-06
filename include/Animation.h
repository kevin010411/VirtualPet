#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>
#include <stddef.h>

enum class AnimationId : uint8_t
{
    None = 0,
    Idle,
    Hungry,
    Depress,
    Sick,
    Dirty,
    Poop,
    Feed,
    Happy,
    Clean,
    Heal,
    Shower,
    PredAnim,
    Gift,
    GiftHappy,
    GuessWin,
    GuessLoss,
    GuessRight,
    GuessWrong,
    Apple1,
    Apple2,
    Apple3,
    AppleP1,
    AppleP2,
    AppleP3,
    AppleP4,
    Predict1,
    Predict2,
    Predict3,
    Predict4,
    Predict5,
    Predict6,
    Predict7,
    Predict8,
    Predict9,
    Predict10,
    Predict11,
    StatusAge01,
    StatusAge05,
    StatusAge1,
    Count
};

constexpr size_t kAnimationIdCount = static_cast<size_t>(AnimationId::Count);

const char *animationIdName(AnimationId id);
AnimationId animationIdFromName(const char *name);

enum class AnimationOwner : uint8_t
{
    BaseState = 0,
    Command = 1,
    Minigame = 2,
    System = 3
};

enum class AnimationPriority : uint8_t
{
    Base = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

struct Animation
{
    AnimationId id;
    unsigned long durationMs;
    bool playOnce;
    AnimationOwner owner;
    AnimationPriority priority;

    Animation(AnimationId animationId = AnimationId::None,
              unsigned long duration = 0,
              bool once = false,
              AnimationOwner animationOwner = AnimationOwner::BaseState,
              AnimationPriority animationPriority = AnimationPriority::Base)
        : id(animationId),
          durationMs(duration),
          playOnce(once),
          owner(animationOwner),
          priority(animationPriority) {}
};

#endif
