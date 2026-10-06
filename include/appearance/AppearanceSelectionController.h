#ifndef APPEARANCE_SELECTION_CONTROLLER_H
#define APPEARANCE_SELECTION_CONTROLLER_H

#include <Arduino.h>
#include "appearance/AppearanceLoader.h"

class Renderer;
class LayoutRenderer;

class AppearanceSelectionController
{
public:
    AppearanceSelectionController(Renderer &renderer, AppearanceLoader &appearanceLoader,
                                  LayoutRenderer &layout);

    bool start(uint8_t speciesSlot, uint8_t currentOutfitSlot, uint8_t unlockMask);
    bool isActive() const;
    void onLeft();
    void onRight();
    bool onConfirm(uint8_t &selectedOutfitSlot, bool &requiresUnlock);
    void exit();
    void requestFullRedraw();
    void render(unsigned long now);

private:
    static constexpr size_t maxOutfitOptions = 8;
    static constexpr unsigned long frameIntervalSlow = 600;

    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    LayoutRenderer &layout;
    bool selectingOutfit = false;
    uint8_t speciesSlot = 1;
    uint8_t unlockMask = 0;
    uint8_t outfitOptions[maxOutfitOptions] = {};
    size_t outfitOptionCount = 0;
    size_t selectedOutfitIndex = 0;
    OutfitPreview selectedOutfitPreview = {};
    bool hasSelectedOutfitPreview = false;
    uint16_t outfitPreviewFrame = 1;
    unsigned long outfitPreviewInterval = frameIntervalSlow;
    unsigned long lastOutfitPreviewFrameTime = 0;
    bool dirtyOutfitPreview = false;

    bool loadSelectedOutfitPreview();
    bool preparePreviewLayout();
    void changeSelection(int delta);
};

#endif // APPEARANCE_SELECTION_CONTROLLER_H
