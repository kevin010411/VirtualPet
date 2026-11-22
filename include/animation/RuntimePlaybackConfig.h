#pragma once

#include "animation/Animation.h"
#include "audio/SequenceAudio.h"

struct RuntimePlaybackConfig
{
    AssetData::AnimationRef systemAnimations[kFirmwarePlaybackRoleCount];
#if ENABLE_SEQUENCE_AUDIO
    SequenceAudioRef animationAudio[AssetData::kMaxRuntimeAnimationId];
#endif
};
