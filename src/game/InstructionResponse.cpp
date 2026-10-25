#include "game/InstructionResponse.h"

#if ENABLE_INSTRUCTION_RESPONSE
void InstructionResponse::start(const InstructionResponseSettings &configuration, RandomBelow source)
{
    settings = configuration;
    random = source;
    completed = 0;
    results = 0;
    completionPending = false;
    inputPending = false;
    heldButtons = 7;
    gesture = false;
    nextRound();
}

void InstructionResponse::nextRound()
{
    const uint32_t total = static_cast<uint32_t>(settings.targetWeights[0]) +
                           settings.targetWeights[1] + settings.targetWeights[2];
    uint32_t draw = random(total);
    target = 0;
    for (; target < 2; ++target)
    {
        if (draw < settings.targetWeights[target]) break;
        draw -= settings.targetWeights[target];
    }
    noGo = settings.noGoEnabled && random(static_cast<uint32_t>(settings.typeWeights[0]) + settings.typeWeights[1]) >= settings.typeWeights[0];
    positions[0] = 0;
    positions[1] = 1;
    positions[2] = 2;
    for (uint8_t count = 3; count > 1; --count)
    {
        const uint8_t other = static_cast<uint8_t>(random(count));
        const uint8_t value = positions[count - 1];
        positions[count - 1] = positions[other];
        positions[other] = value;
    }
    hit = false;
    wrong = false;
    anyInput = false;
    visualReady = false;
    feedbackAnimation = false;
    feedbackAnimationDone = false;
    state = Phase::Preparing;
}

void InstructionResponse::ready(uint32_t now, uint8_t held)
{
    heldButtons = held;
    if (state == Phase::Feedback && !visualReady) began = now;
    visualReady = true;
    if (state == Phase::Preparing && held == 0 && !inputPending)
    {
        began = now;
        state = Phase::Answer;
    }
}

void InstructionResponse::input(uint8_t held, uint8_t pressed, uint32_t timestamp)
{
    heldButtons = held;
    if ((held & 5) == 5)
    {
        if (!gesture)
        {
            gesture = true;
            gestureBegan = timestamp;
        }
    }
    else
    {
        if (gesture && static_cast<uint32_t>(timestamp - gestureBegan) >= 1000)
        {
            cancel();
            return;
        }
        gesture = false;
    }
    if (state == Phase::Preparing && visualReady && held == 0 && !inputPending)
    {
        began = timestamp;
        state = Phase::Answer;
    }
    if (state != Phase::Answer || static_cast<uint32_t>(timestamp - began) >= settings.answerMs)
        return;
    for (uint8_t position = 0; position < 3; ++position)
        if (pressed & (1 << position))
        {
            anyInput = true;
            if (positions[position] == target) hit = true;
            else wrong = true;
        }
}

void InstructionResponse::update(uint32_t now)
{
    if (!active())
        return;
    if (gesture && static_cast<uint32_t>(now - gestureBegan) >= 1000)
    {
        if (inputPending && (pendingRaw & 5) != 5 &&
            static_cast<uint32_t>(pendingAt - gestureBegan) < 1000) return;
        cancel();
        return;
    }
    if (state == Phase::Preparing && visualReady && heldButtons == 0 && !inputPending)
        ready(now, 0);
    if (state == Phase::Answer && static_cast<uint32_t>(now - began) >= settings.answerMs)
    {
        if (inputPending && static_cast<uint32_t>(pendingAt - began) < settings.answerMs) return;
        // Resolve a pending exit before revealing the answer or advancing.
        if (gesture && static_cast<uint32_t>(gestureBegan - began) < settings.answerMs)
            return;
        hit = noGo ? !anyInput : hit && (!settings.hard || !wrong);
        state = Phase::Feedback;
        visualReady = false;
        if (hit) results |= 1UL << completed;
        ++completed;
    }
    else if (state == Phase::Feedback && visualReady &&
             (feedbackAnimation ? feedbackAnimationDone : static_cast<uint32_t>(now - began) >= settings.feedbackMs))
    {
        if (gesture || inputPending)
            return;
        if (completed == settings.roundCount)
        {
            state = Phase::Finished;
            completionPending = true;
        }
        else
            nextRound();
    }
}
#endif
