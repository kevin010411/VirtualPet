#ifndef LAYOUT_RENDERER_H
#define LAYOUT_RENDERER_H

#include <Arduino.h>
#include "animation/domain/Animation.h"

class CommandController;
class Renderer;

class LayoutRenderer
{
public:
    LayoutRenderer(Renderer &renderer, CommandController &commands);

    void begin();
    void drawAll();
    void drawSelection();
    bool enterAction(AnimationId id, int activeSlot);
    bool endAction();
    bool isActionActive() const;

private:
    static constexpr uint8_t maxSlots = 8;
    static constexpr uint8_t tileSize = 32;
    static constexpr uint16_t screenHeight = 160;

    Renderer &renderer;
    CommandController &commands;
    bool actionLayouts[kAnimationIdCount] = {};
    AnimationId activeAction = AnimationId::None;
    int activeActionSlot = -1;

    void loadActionLayouts();
    void drawSlot(int slot, bool selected);
    void drawSlotFromPath(int slot, const char *path);
    bool hasActionLayout(AnimationId id) const;
    static int slotX(int slot);
    static int slotY(int slot);
};

#endif // LAYOUT_RENDERER_H
