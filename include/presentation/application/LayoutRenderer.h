#ifndef LAYOUT_RENDERER_H
#define LAYOUT_RENDERER_H

#include <Arduino.h>
#include "animation/domain/Animation.h"

class CommandController;
class Renderer;
struct PetBehaviorConfig;
struct RuntimeAnimationSceneConfig;

class LayoutRenderer
{
public:
    LayoutRenderer(Renderer &renderer, CommandController &commands);

    void configureRuntimeContract(const PetBehaviorConfig &config);
    void begin();
    void drawAll();
    void drawSelection();
    bool updatePlayback(const AssetData::AnimationRef &animation,
                        uint8_t versionIndex);

private:
    static constexpr uint8_t maxSlots = 8;
    static constexpr uint8_t tileSize = 32;
    static constexpr uint16_t screenHeight = 160;

    Renderer &renderer;
    CommandController &commands;
    const PetBehaviorConfig *runtimeContract = nullptr;
    const RuntimeAnimationSceneConfig *activeScene = nullptr;

    bool drawSlot(int slot, bool selected);
    const RuntimeAnimationSceneConfig *sceneFor(
        const AssetData::AnimationRef &animation,
        uint8_t versionIndex) const;
    static int slotX(int slot);
    static int slotY(int slot);
};

#endif // LAYOUT_RENDERER_H
