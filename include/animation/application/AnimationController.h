#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/application/BaseAnimationRotation.h"
#include "animation/domain/Animation.h"

class Renderer;

class AnimationController
{
public:
    explicit AnimationController(Renderer &renderer);

    void setup(AnimationId baseAnimation);
    void setup(const char *baseAnimation);
    void setBaseAnimation(AnimationId baseAnimation);
    void setBaseAnimation(const char *baseAnimation);
    AnimationId baseAnimation() const;
    bool hasAnimation(AnimationId id) const;
    bool hasNamedAnimation(const char *name) const;
    bool hasActionAnimation(AnimationId id) const;
    bool hasAnimations(const AnimationId *ids, size_t count) const;
    PlaybackResult submit(const AnimationSequence &sequence, PlaybackMode mode);
    void cancelAll();
    bool isBusy() const;
    bool hasAnimationPending(AnimationId id) const;
    AnimationId currentAnimationId() const;
    void requestFullRedraw();
    PlaybackTickResult tick(unsigned long now);
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);
    unsigned long frameIntervalFor(AnimationId id) const;
    unsigned long frameIntervalForName(const char *name) const;
    uint16_t frameCountFor(AnimationId id) const;
    uint16_t frameCountForName(const char *name) const;
    SdFat *sdCard() const;

private:
    static constexpr unsigned long frameIntervalSlow = 600;
    static constexpr uint8_t kMaxQueuedAnimations = 8;

    Renderer &renderer;
    Animation animationQueue[kMaxQueuedAnimations] = {};
    uint8_t animationQueueCount = 0;
    Animation activeAnimation = {};
    bool hasActiveAnimation = false;
    uint8_t activeRepeatsRemaining = 0;
    AnimationId baseAnimationId = AnimationId::Idle;
    bool baseUsesNamedAnimation = false;
    BaseAnimationRotation baseRotation;
    long displayDuration = 0;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    unsigned long lastPlaybackUpdateTime = 0;
    AnimationId showAnimationId = AnimationId::None;
    bool showUsesNamedAnimation = false;
    char showNamedAnimation[32] = {};
    bool playbackFailedThisTick = false;
    AnimationId playbackFailedAnimationIdThisTick = AnimationId::None;

    void resetPlaybackState();
    unsigned long completePlaybackDuration(uint16_t frameCount, unsigned long frameIntervalMs) const;
    void updateElapsed(unsigned long elapsed);
    void completeActiveAnimation();
    void render(unsigned long now);
    void tryStartNextAnimation();
    unsigned long resolvedDuration(const Animation &animation) const;
    PlaybackResult validate(const Animation &animation) const;
    bool hasActionAnimation(const char *baseName) const;
};

#endif // ANIMATION_CONTROLLER_H
