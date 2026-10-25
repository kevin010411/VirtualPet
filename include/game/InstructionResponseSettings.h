#pragma once

#include <stdint.h>

// Web-exported settings are validated before the SD package is emitted.
struct InstructionResponseSettings
{
    uint8_t roundCount = 10;
    uint16_t answerMs = 2000;
    uint16_t feedbackMs = 500;
    uint16_t targetWeights[3] = {1, 1, 1};
    bool hard = false;
    bool noGoEnabled = false;
    uint16_t typeWeights[2] = {1, 1};
};
