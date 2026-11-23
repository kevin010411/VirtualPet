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

#if ENABLE_TRAINING_COURSES
struct TrainingRuntimeCourse
{
    TrainingCourseSettings settings;
    AssetData::AnimationRef frames;
};
#endif

struct InstructionRuntimeSettings
{
#if ENABLE_TRAINING_COURSES
    static constexpr uint8_t kMaxCourses = 3;
    TrainingRuntimeCourse trainingCourses[kMaxCourses];

    const TrainingRuntimeCourse *trainingCourse(TrainingMode mode) const
    {
        for (const auto &course : trainingCourses)
            if (course.settings.mode != TrainingMode::None &&
                (mode == TrainingMode::None || course.settings.mode == mode))
                return &course;
        return nullptr;
    }
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    InstructionResponseSettings instructionSettings;
    AssetData::AnimationRef instructionOptions[3];
    PetBehaviorInstructionEffect instructionEffects[2 * kPetBehaviorSlotCount];
    uint8_t instructionEffectCount;
#endif
};
