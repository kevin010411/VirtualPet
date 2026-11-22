#pragma once

#include "game/CaptureAssets.h"

class Renderer;

namespace CaptureScene
{
static constexpr int bandRows = 8;
static constexpr int bandCount = CaptureAssets::screenHeight / bandRows;
static constexpr int timerHeight = 13;

struct Layer
{
    const CaptureAssets::Sprite *sprite;
    int16_t x, y;
    bool flip;
};

struct Frame
{
    Layer layers[CaptureAssets::maxFish + 1];
    uint16_t secondsLeft;
    uint8_t layerCount;
    uint8_t hookX, lineStart, lineEnd;
};

bool drawBands(Renderer &renderer, CaptureAssets &assets, const Frame &frame, uint32_t bands);
bool showCard(Renderer &renderer, CaptureAssets &assets, const CaptureAssets::Card &card, uint8_t page);
void showScore(Renderer &renderer, int32_t score);
}
