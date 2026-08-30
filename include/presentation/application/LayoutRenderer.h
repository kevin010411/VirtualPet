#ifndef LAYOUT_RENDERER_H
#define LAYOUT_RENDERER_H

#include <Arduino.h>
#include "animation/domain/Animation.h"

class CommandController;
class Renderer;
struct PetBehaviorConfig;
struct RuntimeTableLayoutConfig;

class LayoutRenderer
{
public:
    LayoutRenderer(Renderer &renderer, CommandController &commands);

    void configureRuntimeContract(const PetBehaviorConfig &config);
    void begin();
    void drawAll();
    void drawSelection();
    bool enterAction(FirmwarePlaybackRole id, int activeSlot);
    bool updateAction(FirmwarePlaybackRole id);
    bool endAction();
    bool isActionActive() const;

private:
    static constexpr uint8_t maxSlots = 8;
    static constexpr uint8_t tileSize = 32;
    static constexpr uint16_t screenHeight = 160;

    Renderer &renderer;
    CommandController &commands;
    const PetBehaviorConfig *runtimeContract = nullptr;
    bool actionMode = false;
    FirmwarePlaybackRole activeAction = FirmwarePlaybackRole::None;
    int activeActionSlot = -1;

    bool drawSlot(int slot, bool selected);
    bool hasActionLayout(FirmwarePlaybackRole id) const;
    uint8_t layoutVersion(FirmwarePlaybackRole id) const;
    const RuntimeTableLayoutConfig *layoutFor(FirmwarePlaybackRole id) const;
    static int slotX(int slot);
    static int slotY(int slot);
};

#endif // LAYOUT_RENDERER_H
