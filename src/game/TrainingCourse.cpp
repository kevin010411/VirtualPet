#include "game/TrainingCourse.h"

#if ENABLE_TRAINING_COURSES
void TrainingCourse::start(const TrainingCourseSettings &configuration, RandomBelow source)
{
    settings = configuration;
    random = source;
    completed = score = shown = 0;
    recentChoices = 0;
    lastCorrect = exiting = armed = inputPending = false;
    heldButtons = 7;
    remainingTools = 0;
    results = 0;
    if (settings.mode == TrainingMode::OrderedMemory)
    {
        for (uint8_t index = 0; index < settings.rounds; ++index)
            sequence[index] = index < 3 && settings.rounds >= 3 ? index : random(3);
        for (uint8_t length = settings.rounds; length > 1; --length)
        {
            const auto index = random(length);
            const auto flag = sequence[index];
            sequence[index] = sequence[length - 1];
            sequence[length - 1] = flag;
        }
        enter(Phase::Prompt, sequence[0], settings.cueMs);
        return;
    }
    beginQuestion();
}

void TrainingCourse::enter(Phase next, uint8_t nextFrame, uint16_t duration)
{
    phase = next;
    frame = nextFrame;
    interval = duration;
    redraw = true;
}

void TrainingCourse::beginQuestion()
{
    if (settings.mode == TrainingMode::Timing)
    {
        enter(Phase::Prompt, 0, settings.frameMs + random(260));
        return;
    }
    const uint8_t toolCount = settings.frameCount / 3;
    if (!remainingTools) remainingTools = (1U << toolCount) - 1;
    uint8_t count = 0;
    for (uint8_t tool = 0; tool < toolCount; ++tool)
        if (remainingTools & (1U << tool)) ++count;
    auto pick = random(count);
    uint8_t tool = 0;
    for (;; ++tool)
        if ((remainingTools & (1U << tool)) && pick-- == 0) break;
    remainingTools &= ~(1U << tool);
    targetPosition = random(3);
    enter(Phase::Answer, tool * 3 + targetPosition);
}

void TrainingCourse::ready(uint32_t now)
{
    redraw = false;
    began = now;
}

void TrainingCourse::input(uint8_t held, uint8_t pressed, uint32_t now)
{
    heldButtons = held;
    if ((held & 5) == 5)
    {
        if (!exiting) exitBegan = now;
        exiting = true;
        return;
    }
    if (exiting)
    {
        if (now - exitBegan >= 1000) cancel();
        exiting = false;
        return;
    }
    if (redraw || !armed) return;
    if (settings.mode == TrainingMode::Timing && phase == Phase::Prompt && pressed == 2)
        answer(inCue() && !expired(now));
    else if (phase == Phase::Answer && (pressed == 1 || pressed == 2 || pressed == 4))
    {
        const uint8_t choice = pressed == 4 ? 2 : pressed >> 1;
        if (settings.mode == TrainingMode::Matching)
            answer(choice == targetPosition);
        else
        {
            recentChoices = (recentChoices << 2) | choice;
            answer(choice == sequence[completed]);
        }
    }
}

void TrainingCourse::answer(bool hit)
{
    lastCorrect = hit;
    if (hit)
    {
        results |= 1UL << completed;
        ++score;
    }
    ++completed;
    enter(Phase::Feedback, frame, 280);
}

void TrainingCourse::update(uint32_t now)
{
    if (!active()) return;
    if (exiting)
    {
        if (now - exitBegan >= 1000 &&
            !(inputPending && (pendingRaw & 5) != 5 && pendingAt - exitBegan < 1000))
            cancel();
        return;
    }
    if (!armed && !heldButtons && !inputPending)
    {
        armed = true;
        began = now;
    }
    if (redraw || !armed || !expired(now) || inputPending) return;
    switch (phase)
    {
    case Phase::Prompt:
        if (settings.mode == TrainingMode::Timing)
        {
            if (frame + 1 == settings.frameCount)
                answer(false);
            else
            {
                ++frame;
                enter(Phase::Prompt, frame, inCue() ? settings.cueMs : settings.frameMs);
            }
        }
        else if (frame != 4)
            enter(Phase::Prompt, 4, settings.frameMs);
        else if (++shown == settings.rounds)
            enter(Phase::Answer, 3);
        else
            enter(Phase::Prompt, sequence[shown], settings.cueMs);
        break;
    case Phase::Feedback:
        if (heldButtons) break;
        if (completed == settings.rounds)
            enter(Phase::Result, frame, 1400);
        else if (settings.mode == TrainingMode::OrderedMemory)
            enter(Phase::Answer, 3);
        else
            beginQuestion();
        break;
    case Phase::Result:
        phase = Phase::Finished;
        break;
    default:
        break;
    }
}

bool TrainingCourse::consumeCompletion(uint32_t &mask, uint8_t &count)
{
    if (phase != Phase::Finished) return false;
    mask = results;
    count = completed;
    phase = Phase::Cancelled;
    return true;
}
#endif
