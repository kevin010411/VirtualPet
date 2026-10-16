#include "game/PredictGame.h"

#if ENABLE_COMMAND_PREDICT
#include "common/FirmwareRandom.h"

namespace
{
constexpr FirmwarePlaybackRole kResults[] = {
    FirmwarePlaybackRole::Predict1, FirmwarePlaybackRole::Predict2,
    FirmwarePlaybackRole::Predict3, FirmwarePlaybackRole::Predict4,
    FirmwarePlaybackRole::Predict5, FirmwarePlaybackRole::Predict6,
    FirmwarePlaybackRole::Predict7, FirmwarePlaybackRole::Predict8,
    FirmwarePlaybackRole::Predict9, FirmwarePlaybackRole::Predict10,
    FirmwarePlaybackRole::Predict11};
constexpr size_t kResultCount = sizeof(kResults) / sizeof(kResults[0]);
}

bool PredictGame::canStart(const AnimationController &animations)
{
    return animations.hasAnimation(FirmwarePlaybackRole::PredAnim) &&
           animations.hasAnimations(kResults, kResultCount);
}

bool PredictGame::start()
{
    reset();
    const Animation sequence[] = {
        Animation(FirmwarePlaybackRole::PredAnim, 40000UL, true),
        Animation(kResults[FirmwareRandom::below(kResultCount)], 4800UL, false),
    };
    active = animations.replace(AnimationSequence(sequence, 2)) == PlaybackResult::Accepted;
    return active;
}

void PredictGame::onPlaybackFailed()
{
    animations.cancelAll();
    reset();
}
#endif
