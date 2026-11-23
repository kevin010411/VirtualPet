#pragma once

#include "game/InstructionResponse.h"
#include "game/TrainingCourse.h"

#if ENABLE_INSTRUCTION_RESPONSE
class Renderer;
class AssetResources;
struct InstructionRuntimeSettings;
struct RuntimePlaybackConfig;
struct TrainingRuntimeCourse;

class InstructionPresentation
{
public:
    void start(Renderer &renderer, AssetResources &resources, const InstructionRuntimeSettings &config,
               const RuntimePlaybackConfig &playback, const TrainingRuntimeCourse *training);
    void update();
    void cancel();
    bool active() const;
    void input(uint8_t held, uint8_t pressed, uint32_t timestamp);
    void pendingInput(bool pending, uint32_t at, uint8_t raw);
    bool consumeCompletion(uint32_t &mask, uint8_t &count, TrainingMode &trainingMode);

private:
#if ENABLE_TRAINING_COURSES
    TrainingCourse course;
    bool renderCourse();
    const TrainingRuntimeCourse *activeTraining = nullptr;
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    InstructionResponse instruction;
    InstructionResponse::Phase drawnPhase = InstructionResponse::Phase::Idle;
    AssetResources *instructionResources = nullptr;
    const RuntimePlaybackConfig *instructionPlayback = nullptr;
    const InstructionRuntimeSettings *instructionConfig = nullptr;
    uint8_t instructionHeld = 7;
    uint8_t promptVersion = 0;
    uint32_t frameAt = 0, frameMs = 167;
    bool render();
#endif
    Renderer *instructionRenderer = nullptr;
};
#endif
