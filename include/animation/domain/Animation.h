#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>
#include <stddef.h>
#include "shared/assets/AssetRuntimeContract.h"

enum class FirmwarePlaybackRole : uint8_t
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

constexpr size_t kFirmwarePlaybackRoleCount = static_cast<size_t>(FirmwarePlaybackRole::Count);

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
    FirmwarePlaybackRole playbackRole;
};

struct Animation
{
    FirmwarePlaybackRole playbackRole;
    unsigned long durationMs;
    bool playOnce;
    uint16_t frameIndex;
    uint8_t repeatCount;
    AssetData::AnimationRef asset;
    uint8_t versionIndex;

    Animation(FirmwarePlaybackRole playbackRole = FirmwarePlaybackRole::None,
              unsigned long duration = 0,
              bool once = false,
              uint16_t fixedFrameIndex = 0)
        : playbackRole(playbackRole),
          durationMs(duration),
          playOnce(once),
          frameIndex(fixedFrameIndex),
          repeatCount(1),
          asset{},
          versionIndex(0) {}

    Animation(const AssetData::AnimationRef &animation,
              unsigned long duration,
              bool once,
              uint16_t fixedFrameIndex = 0,
              FirmwarePlaybackRole playbackRole = FirmwarePlaybackRole::None)
        : playbackRole(playbackRole),
          durationMs(duration),
          playOnce(once),
          frameIndex(fixedFrameIndex),
          repeatCount(1),
          asset(animation),
          versionIndex(0) {}

    static Animation complete(FirmwarePlaybackRole playbackRole, uint8_t playbackCount = 1)
    {
        Animation animation(playbackRole, 0, true);
        animation.repeatCount = playbackCount;
        return animation;
    }

    static Animation complete(const AssetData::AnimationRef &animationRef,
                              uint8_t playbackCount = 1,
                              FirmwarePlaybackRole playbackRole = FirmwarePlaybackRole::None)
    {
        Animation animation(animationRef, 0, true, 0, playbackRole);
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
