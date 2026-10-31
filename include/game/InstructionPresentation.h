#pragma once

#include "game/InstructionResponse.h"

#if ENABLE_INSTRUCTION_RESPONSE
class Renderer;
class AssetResources;
struct PetBehaviorConfig;

class InstructionPresentation
{
public:
    void start(Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config);
    void update();
    void cancel() { instruction.cancel(); }
    bool active() const { return instruction.active(); }
    void input(uint8_t held, uint8_t pressed, uint32_t timestamp)
    {
        instructionHeld = held;
        instruction.input(held, pressed, timestamp);
    }
    void pendingInput(bool pending, uint32_t at, uint8_t raw)
    { instruction.pendingInput(pending, at, raw); }
    bool consumeCompletion(uint32_t &mask, uint8_t &count)
    { return instruction.consumeCompletion(mask, count); }

private:
    InstructionResponse instruction;
    InstructionResponse::Phase drawnPhase = InstructionResponse::Phase::Idle;
    Renderer *instructionRenderer = nullptr;
    AssetResources *instructionResources = nullptr;
    const PetBehaviorConfig *instructionConfig = nullptr;
    uint8_t instructionHeld = 7;
    uint8_t promptVersion = 0;
    uint32_t frameAt = 0, frameMs = 167;
    bool render();
};
#endif
