#include "domain/Animation.h"

#include <string.h>

namespace
{
constexpr const char *kAnimationNames[kAnimationIdCount] = {
    "None", "Idle", "Hungry", "Depress", "Sick", "Dirty",
    "Poop", "Feed", "Happy", "Clean", "Heal", "Shower",
    "PredAnim", "Gift", "GiftHappy", "GuessWin", "GuessLoss", "GuessRight",
    "GuessWrong", "GuessItem1", "GuessItem2", "GuessItem3", "GuessLL", "GuessLR",
    "GuessRL", "GuessRR", "Predict1", "Predict2", "Predict3", "Predict4",
    "Predict5", "Predict6", "Predict7", "Predict8", "Predict9", "Predict10",
    "Predict11", "AgeStatus", "Battery", "GuessItem4",
    "GuessStart", "Layout", "LayoutSel"
};
} // namespace

const char *animationIdName(AnimationId id)
{
    const size_t index = static_cast<size_t>(id);
    if (index >= kAnimationIdCount)
        return kAnimationNames[0];
    return kAnimationNames[index];
}

AnimationId animationIdFromName(const char *name)
{
    if (name == nullptr)
        return AnimationId::None;

    for (size_t i = 1; i < kAnimationIdCount; ++i)
    {
        if (strcmp(name, kAnimationNames[i]) == 0)
            return static_cast<AnimationId>(i);
    }
    return AnimationId::None;
}
