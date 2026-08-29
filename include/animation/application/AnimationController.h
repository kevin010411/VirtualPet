#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/application/BaseAnimationRotation.h"
#include "animation/domain/Animation.h"

class Renderer;
struct PetBehaviorConfig;

class AnimationController
{
public:
    explicit AnimationController(Renderer &renderer);

    void configureRuntimeContract(const PetBehaviorConfig &config);
    void setup(const AssetData::AnimationRef &baseAnimation);
    void setBaseAnimation(const AssetData::AnimationRef &baseAnimation);
    AssetData::AnimationRef baseAnimation() const;
    bool hasAnimation(FirmwarePlaybackRole id) const;
    bool hasAnimation(const AssetData::AnimationRef &animation) const;
    bool hasActionAnimation(FirmwarePlaybackRole id) const;
    bool hasAnimations(const FirmwarePlaybackRole *ids, size_t count) const;
    PlaybackResult replace(const AnimationSequence &sequence);
    void cancelAll();
    bool isBusy() const;
    bool hasAnimationPending(FirmwarePlaybackRole id) const;
    FirmwarePlaybackRole currentPlaybackRole() const;
    void requestFullRedraw();
    PlaybackTickResult tick(unsigned long now);
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);
    unsigned long frameIntervalFor(FirmwarePlaybackRole id) const;
    uint16_t frameCountFor(FirmwarePlaybackRole id) const;
    uint16_t frameCountFor(const AssetData::AnimationRef &animation) const;
    SdFat *sdCard() const;

private:
    static constexpr unsigned long frameIntervalSlow = 600;
    static constexpr uint8_t kMaxQueuedAnimations = 8;

    Renderer &renderer;
    Animation animationQueue[kMaxQueuedAnimations] = {};
    uint8_t animationQueueCount = 0;
    Animation activeAnimation = {};
    const PetBehaviorConfig *runtimeContract = nullptr;
    bool hasActiveAnimation = false;
    uint8_t activeRepeatsRemaining = 0;
    AssetData::AnimationRef baseAnimationRef = {};
    BaseAnimationRotation baseRotation;
    long displayDuration = 0;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    unsigned long lastPlaybackUpdateTime = 0;
    FirmwarePlaybackRole showPlaybackRole = FirmwarePlaybackRole::None;
    AssetData::AnimationRef showAnimation = {};
    uint8_t showVersionIndex = 0;
    bool playbackFailedThisTick = false;
    FirmwarePlaybackRole playbackFailedRoleThisTick = FirmwarePlaybackRole::None;

    void resetPlaybackState();
    unsigned long completePlaybackDuration(uint16_t frameCount, unsigned long frameIntervalMs) const;
    void updateElapsed(unsigned long elapsed);
    void completeActiveAnimation();
    void render(unsigned long now);
    void tryStartNextAnimation();
    unsigned long resolvedDuration(const Animation &animation) const;
    PlaybackResult validate(const Animation &animation) const;
    AssetData::AnimationRef systemAnimation(FirmwarePlaybackRole id) const;
    AssetData::AnimationRef resolvedAnimation(const Animation &animation) const;
};

#endif // ANIMATION_CONTROLLER_H
