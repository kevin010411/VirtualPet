#ifndef APPEARANCE_SELECTION_CONTROLLER_H
#define APPEARANCE_SELECTION_CONTROLLER_H

#include <Arduino.h>
#include "appearance/ports/AppearanceLoader.h"

class Renderer;

class AppearanceSelectionController
{
public:
    AppearanceSelectionController(Renderer &renderer, AppearanceLoader &appearanceLoader);

    bool start(uint8_t speciesSlot, uint8_t currentOutfitSlot, uint8_t unlockMask);
    bool startSpecies(uint8_t currentSpeciesSlot);
    bool isActive() const;
    bool isSelectingSpecies() const;
    void onLeft();
    void onRight();
    bool onConfirm(uint8_t &selectedOutfitSlot);
    bool onConfirmSpecies(uint8_t &selectedSpeciesSlot, uint8_t &selectedOutfitSlot);
    void exit();
    void requestFullRedraw();
    void render(unsigned long now);

private:
    static constexpr size_t maxOutfitOptions = 8;
    static constexpr size_t maxSpeciesOptions = 8;
    static constexpr unsigned long frameIntervalSlow = 600;

    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    bool selectingOutfit = false;
    bool selectingSpecies = false;
    uint8_t speciesSlot = 1;
    uint8_t unlockMask = 0;
    uint8_t speciesOptions[maxSpeciesOptions] = {};
    uint8_t outfitOptions[maxOutfitOptions] = {};
    uint8_t speciesDefaultOutfits[maxSpeciesOptions] = {};
    size_t speciesOptionCount = 0;
    size_t outfitOptionCount = 0;
    size_t selectedSpeciesIndex = 0;
    size_t selectedOutfitIndex = 0;
    OutfitPreview selectedOutfitPreview = {};
    bool hasSelectedOutfitPreview = false;
    uint16_t outfitPreviewFrame = 1;
    unsigned long outfitPreviewInterval = frameIntervalSlow;
    unsigned long lastOutfitPreviewFrameTime = 0;
    bool dirtyOutfitPreview = false;

    bool loadSelectedOutfitPreview();
    bool loadSelectedSpeciesPreview();
    void playSelectedChooseAnimation();
    void changeSelection(int delta);
};

#endif // APPEARANCE_SELECTION_CONTROLLER_H
