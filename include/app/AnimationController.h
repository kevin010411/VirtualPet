#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include <Arduino.h>
#include <deque>
#include "domain/Animation.h"

class Renderer;

class AnimationController
{
public:
    explicit AnimationController(Renderer &renderer);

    void setup(AnimationId baseAnimation);
    void setBaseAnimation(AnimationId baseAnimation);
    AnimationId baseAnimation() const;
    bool hasAnimation(AnimationId id) const;
    bool hasAnimations(const AnimationId *ids, size_t count) const;
    void queueAnimation(const Animation &animation);
    void clearByOwner(AnimationOwner owner);
    bool hasAnimationForOwner(AnimationOwner owner) const;
    void markDirty();
    void requestFullRedraw();
    void updateElapsed(unsigned long elapsed);
    void render(unsigned long now);
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);
    unsigned long defaultFrameInterval() const;
    unsigned long frameIntervalFor(AnimationId id) const;
    uint16_t frameCountFor(AnimationId id) const;

private:
    static constexpr unsigned long frameIntervalSlow = 600;

    Renderer &renderer;
    std::deque<Animation> animationQueue = {};
    Animation activeAnimation = {};
    bool hasActiveAnimation = false;
    AnimationId baseAnimationId = AnimationId::Idle;
    long displayDuration = 0;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    AnimationId showAnimationId = AnimationId::None;

    void tryStartNextAnimation();
};

#endif // ANIMATION_CONTROLLER_H
