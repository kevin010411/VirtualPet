#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/domain/Animation.h"

class Renderer;

class AnimationController
{
public:
    explicit AnimationController(Renderer &renderer);

    void setup(AnimationId baseAnimation);
    void setBaseAnimation(AnimationId baseAnimation);
    AnimationId baseAnimation() const;
    bool hasAnimation(AnimationId id) const;
    bool hasNamedAnimation(const char *name) const;
    bool hasActionAnimation(AnimationId id) const;
    bool hasActionAnimation(const char *baseName) const;
    bool hasAnimations(const AnimationId *ids, size_t count) const;
    void queueAnimation(const Animation &animation);
    void queueNamedAnimation(const char *name,
                             unsigned long durationMs,
                             bool playOnce,
                             AnimationOwner owner,
                             AnimationPriority priority,
                             uint16_t fixedFrameIndex = 0);
    bool queueActionAnimation(AnimationId id,
                              unsigned long durationMs,
                              bool playOnce,
                              AnimationOwner owner,
                              AnimationPriority priority);
    bool queueActionAnimation(const char *baseName,
                              unsigned long durationMs,
                              bool playOnce,
                              AnimationOwner owner,
                              AnimationPriority priority);
    bool queueActionAnimation(AnimationId id,
                              unsigned long durationMs,
                              bool playOnce,
                              AnimationOwner owner,
                              AnimationPriority priority,
                              char *selectedName,
                              size_t selectedNameSize);
    bool queueActionAnimation(const char *baseName,
                              unsigned long durationMs,
                              bool playOnce,
                              AnimationOwner owner,
                              AnimationPriority priority,
                              char *selectedName,
                              size_t selectedNameSize);
    void clearByOwner(AnimationOwner owner);
    bool hasAnimationForOwner(AnimationOwner owner) const;
    AnimationId currentCommandAnimationId() const;
    void markDirty();
    void requestFullRedraw();
    void showResourceError();
    void showStatusNotFound();
    void updateElapsed(unsigned long elapsed);
    void render(unsigned long now);
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);
    unsigned long defaultFrameInterval() const;
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
    AnimationId baseAnimationId = AnimationId::Idle;
    long displayDuration = 0;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    AnimationId showAnimationId = AnimationId::None;
    bool showUsesNamedAnimation = false;
    char showNamedAnimation[32] = {};

    void tryStartNextAnimation();
};

#endif // ANIMATION_CONTROLLER_H
