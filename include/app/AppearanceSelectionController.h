#ifndef APPEARANCE_SELECTION_CONTROLLER_H
#define APPEARANCE_SELECTION_CONTROLLER_H

#include <Arduino.h>
#include "storage/AppearanceLoader.h"

class Renderer;

class AppearanceSelectionController
{
public:
    AppearanceSelectionController(Renderer &renderer, AppearanceLoader &appearanceLoader);

    bool start(const char *speciesCode, const char *currentOutfitCode);
    bool startSpecies(const char *currentSpeciesCode);
    bool isActive() const;
    bool isSelectingSpecies() const;
    void onLeft();
    void onRight();
    bool onConfirm(char *selectedOutfit, size_t selectedOutfitSize);
    bool onConfirmSpecies(char *selectedSpecies, size_t selectedSpeciesSize, char *selectedOutfit, size_t selectedOutfitSize);
    void exit();
    void render(unsigned long now);

private:
    static constexpr size_t maxOutfitOptions = 8;
    static constexpr size_t maxSpeciesOptions = 8;
    static constexpr unsigned long frameIntervalSlow = 600;

    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    bool selectingOutfit = false;
    bool selectingSpecies = false;
    char speciesCode[9] = "dino";
    char speciesOptions[maxSpeciesOptions][9] = {};
    char outfitOptions[maxOutfitOptions][9] = {};
    char speciesDefaultOutfits[maxSpeciesOptions][9] = {};
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
    void changeSelection(int delta);
};

#endif // APPEARANCE_SELECTION_CONTROLLER_H
