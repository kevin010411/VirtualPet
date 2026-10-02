#ifndef LAYOUT_RENDERER_H
#define LAYOUT_RENDERER_H

#include <Arduino.h>
#include "animation/domain/Animation.h"

class CommandController;
class Renderer;
struct PetBehaviorConfig;

class LayoutRenderer
{
public:
    LayoutRenderer(Renderer &renderer, CommandController &commands);

    void configureRuntimeContract(const PetBehaviorConfig &config);
    void begin();
    void drawAll();
    void drawSelection();
    bool updatePlayback(uint8_t layoutId);

private:
    Renderer &renderer;
    CommandController &commands;
    const PetBehaviorConfig *runtimeContract = nullptr;
    uint8_t activeLayoutId = 0;
    bool hasActiveLayout = false;

    bool drawSlot(int slot, bool selected);
};

#endif // LAYOUT_RENDERER_H
