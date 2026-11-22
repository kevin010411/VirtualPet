#pragma once

#include "game/InstructionResponseSettings.h"
#include "game/TrainingCourse.h"
#include "pet/PetBehaviorTypes.h"

#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
struct PetBehaviorInstructionEffect
{
    uint8_t group;
    uint8_t statSlot;
    PetBehaviorEffectOperation operation;
    int64_t value;
};
#endif

struct InstructionRuntimeSettings
{
#if ENABLE_TRAINING_COURSES
    TrainingCourseSettings trainingSettings;
    AssetData::AnimationRef trainingFrames;
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    InstructionResponseSettings instructionSettings;
    AssetData::AnimationRef instructionOptions[3];
    PetBehaviorInstructionEffect instructionEffects[2 * kPetBehaviorSlotCount];
    uint8_t instructionEffectCount;
#endif
};
