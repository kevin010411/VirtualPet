#pragma once

#include <stdint.h>
#include "common/AppProfile.h"
#include "game/InstructionResponseSettings.h"

#if ENABLE_INSTRUCTION_RESPONSE
// Input masks follow screen positions: left=1, centre=2, right=4.
class InstructionResponse
{
public:
    enum class Phase : uint8_t { Idle, Preparing, Answer, Feedback, Finished, Cancelled };
    using RandomBelow = uint32_t (*)(uint32_t);

    void start(const InstructionResponseSettings &settings, RandomBelow random);
    void ready(uint32_t now, uint8_t held);
    void waitForFeedbackAnimation() { feedbackAnimation = true; feedbackAnimationDone = false; }
    void finishFeedbackAnimation() { feedbackAnimationDone = true; }
    void input(uint8_t held, uint8_t pressed, uint32_t timestamp);
    void pendingInput(bool pending, uint32_t at, uint8_t raw)
    { inputPending = pending; pendingAt = at; pendingRaw = raw; }
    void update(uint32_t now);
    void cancel() { state = Phase::Cancelled; completed = 0; results = 0; completionPending = false; }
    bool consumeCompletion(uint32_t &correctMask, uint8_t &count)
    {
        if (!completionPending) return false;
        completionPending = false;
        correctMask = results;
        count = completed;
        return true;
    }
    Phase phase() const { return state; }
    bool active() const
    {
        return state == Phase::Preparing || state == Phase::Answer || state == Phase::Feedback;
    }
    uint8_t itemAt(uint8_t position) const { return positions[position]; }
    uint8_t targetItem() const { return target; }
    bool correct() const { return hit; }
    bool isNoGo() const { return noGo; }
    uint8_t completedRounds() const { return completed; }

private:
    void nextRound();
    InstructionResponseSettings settings;
    RandomBelow random = nullptr;
    Phase state = Phase::Idle;
    uint8_t positions[3] = {0, 1, 2};
    uint8_t target = 0;
    uint8_t completed = 0;
    uint32_t results = 0;
    bool completionPending = false;
    uint32_t began = 0;
    uint32_t gestureBegan = 0;
    bool hit = false;
    bool wrong = false;
    bool anyInput = false;
    bool noGo = false;
    bool visualReady = false;
    bool feedbackAnimation = false;
    bool feedbackAnimationDone = false;
    bool gesture = false;
    bool inputPending = false;
    uint8_t heldButtons = 7, pendingRaw = 0;
    uint32_t pendingAt = 0;
};
#endif
