#ifndef PREDICT_GAME_H
#define PREDICT_GAME_H

#include "common/AppProfile.h"

#if ENABLE_COMMAND_PREDICT
#include "animation/AnimationController.h"

class PredictGame
{
public:
    explicit PredictGame(AnimationController &animations) : animations(animations) {}
    static bool canStart(const AnimationController &animations);
    bool start();
    void reset() { active = false; }
    void update() { if (active && !animations.isBusy()) active = false; }
    bool isActive() const { return active; }
    void onPlaybackFailed();

private:
    AnimationController &animations;
    bool active = false;
};
#endif

#endif
