#include "animation/RuntimePlaybackConfig.h"
#include "game/InstructionPresentation.h"
#if ENABLE_INSTRUCTION_RESPONSE
#include "resources/AssetResources.h"
#include "display/Renderer.h"
#include "game/InstructionRuntimeSettings.h"
#include "common/FirmwareRandom.h"

void InstructionPresentation::start(Renderer &renderer, AssetResources &resources, const InstructionRuntimeSettings &config,
                                    const RuntimePlaybackConfig &playback, const TrainingRuntimeCourse *training)
{
    instructionRenderer = &renderer;
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    instructionPlayback = &playback;
    instructionConfig = &config;
#else
    (void)playback;
    (void)config;
#endif
#if ENABLE_TRAINING_COURSES
    activeTraining = training;
    if (activeTraining != nullptr)
    {
        course.start(activeTraining->settings, FirmwareRandom::belowWide);
        return;
    }
#else
    (void)training;
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    instructionResources = &resources;
    instructionHeld = 7;
    drawnPhase = InstructionResponse::Phase::Idle;
    auto settings = config.instructionSettings;
    settings.feedbackMs = 500;
    instruction.start(settings, FirmwareRandom::belowWide);
#else
    (void)resources;
#endif
}

void InstructionPresentation::update()
{
#if ENABLE_TRAINING_COURSES
    if (activeTraining != nullptr)
    {
        course.update(millis());
        if (course.active() && course.redraw && !renderCourse())
        {
            course.cancel();
            instructionRenderer->showResourceError();
        }
        return;
    }
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    instruction.update(millis());
    if (instruction.active() && !render())
    {
        instruction.cancel();
        instructionRenderer->showResourceError();
    }
#endif
}

#if ENABLE_TRAINING_COURSES
bool InstructionPresentation::renderCourse()
{
    Renderer &renderer = *instructionRenderer;
    using Phase = TrainingCourse::Phase;
    const auto &settings = activeTraining->settings;
    const bool answerArtwork = settings.hasAnswerArtwork();
    if (course.phase == Phase::Result)
        renderer.showTrainingResult(course.score, course.completed, course.score >= settings.passScore);
    else if (course.phase == Phase::Feedback && !answerArtwork)
        renderer.showInstructionFeedback(course.lastCorrect);
    else
    {
        if (!renderer.ShowAnimationFrame(activeTraining->frames, 0, course.frame + 1,
                                         0, 0, 12, 128, 160))
            return false;
        if (answerArtwork && (course.phase == Phase::Answer || course.phase == Phase::Feedback))
        {
            const uint8_t patchSize = settings.customRecall ? 12 : 16;
            uint8_t cell = course.completed > kOrderedMemoryRecallCells ? kOrderedMemoryRecallCells : course.completed;
            uint16_t choices = course.recentChoices;
            while (cell)
            {
                --cell;
                const uint8_t patch = cell * kOrderedMemoryAnswerChoices + (choices & 3) + 1;
                const uint8_t x = settings.customRecall ? settings.recallPositions[cell][0] : 18 + 19 * cell;
                const uint8_t y = settings.customRecall ? settings.recallPositions[cell][1] : 80;
                if (!renderer.ShowAnimationFrame(activeTraining->frames, 1, patch,
                                                 x, y, 4, patchSize, patchSize))
                    return false;
                choices >>= 2;
            }
        }
        else if (course.phase == Phase::Answer && settings.mode == TrainingMode::OrderedMemory)
            renderer.showTrainingRecall(course.recentChoices, course.completed);
    }
    course.ready(millis());
    return true;
}
#endif

void InstructionPresentation::cancel()
{
#if ENABLE_TRAINING_COURSES
    if (activeTraining != nullptr) { course.cancel(); return; }
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    instruction.cancel();
#endif
}

bool InstructionPresentation::active() const
{
#if ENABLE_TRAINING_COURSES
    if (activeTraining != nullptr) return course.active();
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    return instruction.active();
#else
    return false;
#endif
}

void InstructionPresentation::input(uint8_t held, uint8_t pressed, uint32_t timestamp)
{
#if ENABLE_TRAINING_COURSES
    if (activeTraining != nullptr) { course.input(held, pressed, timestamp); return; }
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    instructionHeld = held;
    instruction.input(held, pressed, timestamp);
#endif
}

void InstructionPresentation::pendingInput(bool pending, uint32_t at, uint8_t raw)
{
#if ENABLE_TRAINING_COURSES
    if (activeTraining != nullptr) { course.pendingInput(pending, at, raw); return; }
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    instruction.pendingInput(pending, at, raw);
#endif
}

bool InstructionPresentation::consumeCompletion(uint32_t &mask, uint8_t &count, TrainingMode &trainingMode)
{
    trainingMode = TrainingMode::None;
#if ENABLE_TRAINING_COURSES
    if (activeTraining != nullptr)
    {
        trainingMode = activeTraining->settings.mode;
        return course.consumeCompletion(mask, count);
    }
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    return instruction.consumeCompletion(mask, count);
#else
    return false;
#endif
}

#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
bool InstructionPresentation::render()
{
    const auto phase = instruction.phase();
    const uint32_t now = millis();
    Renderer &renderer = *instructionRenderer;
    const bool changed = drawnPhase != phase &&
        !(drawnPhase == InstructionResponse::Phase::Preparing && phase == InstructionResponse::Phase::Answer);
    const auto feedbackRole = instruction.correct() ? FirmwarePlaybackRole::InstructionCorrect :
                                                     FirmwarePlaybackRole::InstructionWrong;
    if (changed)
    {
        AssetData::AnimationRef prompt = {};
        if (phase == InstructionResponse::Phase::Preparing)
        {
            renderer.setAnimationArea(0, 0, 128, 128);
            renderer.clearInstructionOptions();
            for (uint8_t position = 0; position < 3; ++position)
                if (!renderer.ShowAnimationFrame(
                        instructionConfig->instructionOptions[instruction.itemAt(position)],
                        0, 1, position * 43, 128, 12, 42, 32))
                    return false;
            prompt = instructionPlayback->systemAnimations[
                static_cast<uint8_t>(instruction.isNoGo() ? FirmwarePlaybackRole::InstructionNoGo1 : FirmwarePlaybackRole::InstructionPrompt1) + instruction.targetItem()];
        }
        else if (phase == InstructionResponse::Phase::Feedback)
        {
            renderer.clearInstructionPromptTop();
            renderer.setAnimationArea(0, 32, 128, 96);
            prompt = instructionPlayback->systemAnimations[static_cast<uint8_t>(feedbackRole)];
        }
        if (prompt.valid())
        {
            promptVersion = static_cast<uint8_t>(FirmwareRandom::below(instructionResources->versionCountFor(prompt)));
            frameMs = instructionResources->frameIntervalFor(prompt, promptVersion, 167);
            if (phase == InstructionResponse::Phase::Feedback)
                instruction.waitForFeedbackAnimation();
            if (!renderer.setAnimation(prompt, promptVersion, phase == InstructionResponse::Phase::Feedback))
                return false;
            renderer.advanceAnimationFrame();
            if (renderer.animationFrameFailed())
                return false;
        }
        else if (phase == InstructionResponse::Phase::Feedback)
            renderer.showInstructionFeedback(instruction.correct());
        else
            return false;
        frameAt = millis();
        drawnPhase = phase;
        if (phase == InstructionResponse::Phase::Preparing || phase == InstructionResponse::Phase::Feedback)
            instruction.ready(millis(), instructionHeld);
        return true;
    }
    drawnPhase = phase;
    const bool animated = phase != InstructionResponse::Phase::Feedback ||
        instructionPlayback->systemAnimations[static_cast<uint8_t>(feedbackRole)].valid();
    if (animated && static_cast<uint32_t>(now - frameAt) >= frameMs)
    {
        const bool complete = renderer.advanceAnimationFrame();
        if (renderer.animationFrameFailed())
            return false;
        frameAt = millis();
        if (phase == InstructionResponse::Phase::Feedback && complete)
            instruction.finishFeedbackAnimation();
    }
    return true;
}
#endif
#endif
