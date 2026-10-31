#include "animation/BaseAnimationRotation.h"

#include <string.h>
#include "resources/AssetResources.h"
#include "common/FirmwareRandom.h"


void BaseAnimationRotation::reset()
{
    baseAnimation = {};
    selectedVersionIndex = 0;
    completedLoops = 0;
}

bool BaseAnimationRotation::setBaseAnimation(const AssetData::AnimationRef &animation,
                                             AssetResources &resources)
{
    if (!animation.valid() ||
        (baseAnimation.speciesSlot == animation.speciesSlot &&
         baseAnimation.outfitSlot == animation.outfitSlot &&
         baseAnimation.animationId == animation.animationId))
        return false;
    uint8_t version = 0;
    if (!selectVersion(animation, resources, version))
        return false;
    baseAnimation = animation;
    selectedVersionIndex = version;
    completedLoops = 0;
    return true;
}

bool BaseAnimationRotation::onLoopCompletedAndRotateIfDue(AssetResources &resources)
{
    if (!baseAnimation.valid() || ++completedLoops < kLoopsPerSelection)
        return false;
    completedLoops = 0;
    uint8_t nextVersion = 0;
    if (!selectVersion(baseAnimation, resources, nextVersion))
        return false;
    const bool changed = nextVersion != selectedVersionIndex;
    selectedVersionIndex = nextVersion;
    return changed;
}

AssetData::AnimationRef BaseAnimationRotation::selectedAnimation() const
{
    return baseAnimation;
}

uint8_t BaseAnimationRotation::selectedVersion() const
{
    return selectedVersionIndex;
}

bool BaseAnimationRotation::selectVersion(const AssetData::AnimationRef &animation,
                                          AssetResources &resources,
                                          uint8_t &versionIndex) const
{
    const uint16_t count = resources.versionCountFor(animation);
    if (count == 0)
        return false;
    versionIndex = count == 1 ? 0 : static_cast<uint8_t>(FirmwareRandom::below(count));
    return true;
}
