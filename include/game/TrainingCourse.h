#pragma once

#include "common/AppProfile.h"
#include <stdint.h>

#if ENABLE_TRAINING_COURSES
enum class TrainingMode : uint8_t { None, Timing, OrderedMemory, Matching };

struct TrainingCourseSettings
{
    TrainingMode mode = TrainingMode::None;
    uint8_t gateSlot = 255;
    uint8_t completionSlot = 0;
    uint8_t scoreSlot = 0;
    uint32_t gateValue = 0;
    uint8_t rounds = 0;
    uint8_t cueStart = 0;
    uint8_t cueEnd = 0;
    uint8_t passScore = 0;
    uint8_t frameCount = 0;
    uint16_t frameMs = 0;
    uint16_t cueMs = 0;
};

class TrainingCourse
{
public:
    enum class Phase : uint8_t { Prompt, Answer, Feedback, Result, Finished, Cancelled };
    using RandomBelow = uint32_t (*)(uint32_t);

    void start(const TrainingCourseSettings &configuration, RandomBelow source);
    void ready(uint32_t now);
    void input(uint8_t held, uint8_t pressed, uint32_t now);
    void pendingInput(bool pending, uint32_t at, uint8_t raw)
    {
        inputPending = pending;
        pendingAt = at;
        pendingRaw = raw;
    }
    void update(uint32_t now);
    void cancel() { phase = Phase::Cancelled; }
    bool active() const { return phase < Phase::Finished; }
    bool consumeCompletion(uint32_t &mask, uint8_t &count);

    Phase phase = Phase::Cancelled;
    uint8_t frame = 0;
    uint8_t completed = 0;
    uint8_t score = 0;
    uint16_t recentChoices = 0;
    bool lastCorrect = false;
    bool redraw = false;

private:
    void beginQuestion();
    void enter(Phase next, uint8_t nextFrame, uint16_t duration = 0);
    void answer(bool hit);
    bool inCue() const { return frame >= settings.cueStart && frame < settings.cueEnd; }
    bool expired(uint32_t now) const { return now - began >= interval; }
    TrainingCourseSettings settings;
    RandomBelow random = nullptr;
    uint8_t sequence[20] = {};
    uint8_t shown = 0;
    uint8_t targetPosition = 0;
    uint8_t heldButtons = 7;
    uint16_t remainingTools = 0;
    uint16_t interval = 0;
    uint32_t began = 0;
    uint32_t exitBegan = 0;
    uint32_t results = 0;
    bool exiting = false;
    bool armed = false;
    bool inputPending = false;
    uint32_t pendingAt = 0;
    uint8_t pendingRaw = 0;
};
#endif
