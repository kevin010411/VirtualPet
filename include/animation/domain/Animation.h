#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

enum class AnimationId : uint8_t
{
    None = 0,
    Idle,
    PredAnim,
    GuessWin,
    GuessLoss,
    GuessRight,
    GuessWrong,
    GuessItem1,
    GuessItem2,
    GuessItem3,
    GuessLL,
    GuessLR,
    GuessRL,
    GuessRR,
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
    Status,
    Battery,
    GuessItem4,
    GuessStart,
    Layout,
    LayoutSel,
    Start,
    Evolution,
    StartIntro,
    FirstStart,
    Count
};

constexpr size_t kAnimationIdCount = static_cast<size_t>(AnimationId::Count);

AnimationId animationIdFromName(const char *name);
const char *animationNameFromId(AnimationId id);

enum class PlaybackResult : uint8_t
{
    Accepted,
    AnimationMissing,
    QueueFull,
    PlaybackFailed,
};

struct PlaybackTickResult
{
    PlaybackResult result;
    AnimationId animationId;
};

struct Animation
{
    AnimationId id;
    unsigned long durationMs;
    bool playOnce;
    uint16_t frameIndex;
    uint8_t repeatCount;
    bool usesNamedAnimation;
    char namedAnimation[32];

    Animation(AnimationId animationId = AnimationId::None,
              unsigned long duration = 0,
              bool once = false,
              uint16_t fixedFrameIndex = 0)
        : id(animationId),
          durationMs(duration),
          playOnce(once),
          frameIndex(fixedFrameIndex),
          repeatCount(1),
          usesNamedAnimation(false),
          namedAnimation{} {}

    Animation(const char *animationName,
              unsigned long duration,
              bool once,
              uint16_t fixedFrameIndex = 0)
        : id(AnimationId::None),
          durationMs(duration),
          playOnce(once),
          frameIndex(fixedFrameIndex),
          repeatCount(1),
          usesNamedAnimation(true),
          namedAnimation{}
    {
        if (animationName != nullptr)
        {
            if (strlen(animationName) >= sizeof(namedAnimation))
                return;
            strncpy(namedAnimation, animationName, sizeof(namedAnimation) - 1);
            namedAnimation[sizeof(namedAnimation) - 1] = '\0';
        }
    }

    static Animation complete(AnimationId animationId, uint8_t playbackCount = 1)
    {
        Animation animation(animationId, 0, true);
        animation.repeatCount = playbackCount;
        return animation;
    }

    static Animation complete(const char *animationName, uint8_t playbackCount = 1)
    {
        Animation animation(animationName, 0, true);
        animation.repeatCount = playbackCount;
        return animation;
    }

    bool isFixedFrame() const { return frameIndex != 0; }
    bool usesAutomaticDuration() const { return playOnce && durationMs == 0 && !isFixedFrame(); }
};

struct AnimationSequence
{
    const Animation *items;
    uint8_t count;

    AnimationSequence(const Animation *sequenceItems, uint8_t sequenceCount)
        : items(sequenceItems), count(sequenceCount) {}
};

#endif
