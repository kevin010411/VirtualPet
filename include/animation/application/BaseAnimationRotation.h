#ifndef BASE_ANIMATION_ROTATION_H
#define BASE_ANIMATION_ROTATION_H

#include <Arduino.h>
#include "shared/assets/AssetRuntimeContract.h"

class Renderer;

class BaseAnimationRotation
{
public:
    void reset();
    bool setBaseAnimation(const AssetData::AnimationRef &baseAnimation, Renderer &renderer);
    bool onLoopCompletedAndRotateIfDue(Renderer &renderer);
    AssetData::AnimationRef selectedAnimation() const;
    uint8_t selectedVersion() const;

private:
    static constexpr uint8_t kLoopsPerSelection = 10;
    AssetData::AnimationRef baseAnimation = {};
    uint8_t selectedVersionIndex = 0;
    uint8_t completedLoops = 0;

    bool selectVersion(const AssetData::AnimationRef &animation,
                       Renderer &renderer,
                       uint8_t &versionIndex) const;
};

#endif // BASE_ANIMATION_ROTATION_H
