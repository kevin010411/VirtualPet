#include "game/InstructionPresentation.h"
#if ENABLE_INSTRUCTION_RESPONSE
#include "resources/AssetResources.h"
#include "display/Renderer.h"
#include "pet/PetBehaviorTypes.h"
#include "common/FirmwareRandom.h"

void InstructionPresentation::start(Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config)
{
    instructionRenderer = &renderer;
    instructionResources = &resources;
    instructionConfig = &config;
    instructionHeld = 7;
    drawnPhase = InstructionResponse::Phase::Idle;
    auto settings = config.instructionSettings;
    settings.feedbackMs = 500;
    instruction.start(settings, FirmwareRandom::belowWide);
}

void InstructionPresentation::update()
{
    instruction.update(millis());
    if (instruction.active() && !render())
    {
        instruction.cancel();
        instructionRenderer->showResourceError();
    }
}

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
        renderer.setAnimationArea(0, 32, 128, 96);
        AssetData::AnimationRef prompt = {};
        if (phase == InstructionResponse::Phase::Preparing)
        {
            renderer.clearInstructionOptions();
            for (uint8_t position = 0; position < 3; ++position)
                if (!renderer.ShowAnimationFrame(
                        instructionConfig->instructionOptions[instruction.itemAt(position)],
                        0, 1, position * 43, 128, 12, 42, 32))
                    return false;
            prompt = instructionConfig->systemAnimations[
                static_cast<uint8_t>(instruction.isNoGo() ? FirmwarePlaybackRole::InstructionNoGo1 : FirmwarePlaybackRole::InstructionPrompt1) + instruction.targetItem()];
        }
        else if (phase == InstructionResponse::Phase::Feedback)
            prompt = instructionConfig->systemAnimations[static_cast<uint8_t>(feedbackRole)];
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
        instructionConfig->systemAnimations[static_cast<uint8_t>(feedbackRole)].valid();
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
