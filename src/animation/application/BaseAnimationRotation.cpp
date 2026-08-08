#include "animation/application/BaseAnimationRotation.h"

#include <string.h>
#include "presentation/adapters/rendering/Renderer.h"

void BaseAnimationRotation::reset()
{
    baseAnimationName[0] = '\0';
    selectedAnimationName[0] = '\0';
    completedLoops = 0;
}

bool BaseAnimationRotation::setBaseAnimation(const char *baseAnimation, const Renderer &renderer)
{
    if (baseAnimation == nullptr || baseAnimation[0] == '\0' ||
        strlen(baseAnimation) >= sizeof(baseAnimationName) ||
        strcmp(baseAnimationName, baseAnimation) == 0)
    {
        return false;
    }

    char selected[kAnimationNameSize] = {};
    if (!selectVersion(baseAnimation, renderer, selected, sizeof(selected)))
        return false;

    strncpy(baseAnimationName, baseAnimation, sizeof(baseAnimationName) - 1);
    baseAnimationName[sizeof(baseAnimationName) - 1] = '\0';
    strncpy(selectedAnimationName, selected, sizeof(selectedAnimationName) - 1);
    selectedAnimationName[sizeof(selectedAnimationName) - 1] = '\0';
    completedLoops = 0;
    return true;
}

bool BaseAnimationRotation::onLoopCompletedAndRotateIfDue(const Renderer &renderer)
{
    if (baseAnimationName[0] == '\0' || ++completedLoops < kLoopsPerSelection)
        return false;

    completedLoops = 0;
    char selected[kAnimationNameSize] = {};
    if (!selectVersion(baseAnimationName, renderer, selected, sizeof(selected)))
        return false;

    const bool selectionChanged = strcmp(selectedAnimationName, selected) != 0;
    strncpy(selectedAnimationName, selected, sizeof(selectedAnimationName) - 1);
    selectedAnimationName[sizeof(selectedAnimationName) - 1] = '\0';
    return selectionChanged;
}

const char *BaseAnimationRotation::selectedAnimation() const
{
    return selectedAnimationName;
}

bool BaseAnimationRotation::selectVersion(const char *baseAnimation,
                                          const Renderer &renderer,
                                          char *destination,
                                          size_t destinationSize) const
{
    const uint8_t variantCount = renderer.variantCountFor(baseAnimation);
    const char *selected = variantCount > 0
                               ? renderer.variantNameFor(baseAnimation, static_cast<uint8_t>(random(variantCount)))
                               : baseAnimation;
    if (selected == nullptr || selected[0] == '\0' || strlen(selected) >= destinationSize)
        return false;

    strncpy(destination, selected, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
    return true;
}
