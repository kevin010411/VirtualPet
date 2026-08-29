#include "appearance/application/AppearanceSelectionController.h"

#include <string.h>
#include "shared/config/AppProfile.h"
#include "presentation/adapters/rendering/Renderer.h"

AppearanceSelectionController::AppearanceSelectionController(Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef)
{
}

bool AppearanceSelectionController::start(uint8_t sourceSpeciesSlot, uint8_t currentOutfitSlot)
{
    selectingSpecies = false;
    speciesSlot = sourceSpeciesSlot;
    outfitOptionCount = 0;
    selectedOutfitIndex = 0;
    hasSelectedOutfitPreview = false;

    if (!appearanceLoader.loadOutfits(speciesSlot, outfitOptions, maxOutfitOptions, outfitOptionCount))
        return false;

    for (size_t i = 0; i < outfitOptionCount; ++i)
    {
        if (outfitOptions[i] == currentOutfitSlot)
        {
            selectedOutfitIndex = i;
            break;
        }
    }

    if (outfitOptionCount == 0)
        return false;

    selectingOutfit = true;
    outfitPreviewFrame = 1;
    lastOutfitPreviewFrameTime = 0;
    dirtyOutfitPreview = true;
    loadSelectedOutfitPreview();
    return true;
}

bool AppearanceSelectionController::startSpecies(uint8_t currentSpeciesSlot)
{
    selectingOutfit = false;
    speciesOptionCount = 0;
    selectedSpeciesIndex = 0;
    hasSelectedOutfitPreview = false;

    if (!appearanceLoader.loadSpecies(speciesOptions, maxSpeciesOptions, speciesOptionCount))
        return false;

    for (size_t i = 0; i < speciesOptionCount; ++i)
    {
        size_t defaultOutfitCount = 0;
        speciesDefaultOutfits[i] = 0;
        appearanceLoader.loadOutfits(speciesOptions[i], &speciesDefaultOutfits[i], 1, defaultOutfitCount);
        if (speciesOptions[i] == currentSpeciesSlot)
            selectedSpeciesIndex = i;
    }

    if (speciesOptionCount == 0)
        return false;

    selectingSpecies = true;
    outfitPreviewFrame = 1;
    lastOutfitPreviewFrameTime = 0;
    dirtyOutfitPreview = true;
    loadSelectedSpeciesPreview();
    return true;
}

bool AppearanceSelectionController::isActive() const
{
    return selectingOutfit || selectingSpecies;
}

bool AppearanceSelectionController::isSelectingSpecies() const
{
    return selectingSpecies;
}

void AppearanceSelectionController::onLeft()
{
    changeSelection(-1);
}

void AppearanceSelectionController::onRight()
{
    changeSelection(1);
}

bool AppearanceSelectionController::onConfirm(uint8_t &selectedOutfitSlot)
{
    if (outfitOptionCount == 0 || selectedOutfitIndex >= outfitOptionCount)
    {
        exit();
        return false;
    }

    selectedOutfitSlot = outfitOptions[selectedOutfitIndex];
    playSelectedChooseAnimation();
    exit();
    return true;
}

bool AppearanceSelectionController::onConfirmSpecies(uint8_t &selectedSpeciesSlot,
                                                     uint8_t &selectedOutfitSlot)
{
    if (speciesOptionCount == 0 || selectedSpeciesIndex >= speciesOptionCount ||
        speciesDefaultOutfits[selectedSpeciesIndex] == 0)
    {
        exit();
        return false;
    }

    selectedSpeciesSlot = speciesOptions[selectedSpeciesIndex];
    selectedOutfitSlot = speciesDefaultOutfits[selectedSpeciesIndex];
    playSelectedChooseAnimation();
    exit();
    return true;
}

void AppearanceSelectionController::exit()
{
    selectingOutfit = false;
    selectingSpecies = false;
    speciesOptionCount = 0;
    outfitOptionCount = 0;
    selectedSpeciesIndex = 0;
    selectedOutfitIndex = 0;
    hasSelectedOutfitPreview = false;
    selectedOutfitPreview = {};
    outfitPreviewFrame = 1;
    outfitPreviewInterval = frameIntervalSlow;
    lastOutfitPreviewFrameTime = 0;
    dirtyOutfitPreview = false;
}

void AppearanceSelectionController::requestFullRedraw()
{
    dirtyOutfitPreview = true;
    lastOutfitPreviewFrameTime = 0;
}

void AppearanceSelectionController::render(unsigned long now)
{
    if (!hasSelectedOutfitPreview)
        return;

    const bool frameDue = dirtyOutfitPreview || lastOutfitPreviewFrameTime == 0 || now - lastOutfitPreviewFrameTime >= outfitPreviewInterval;
    if (!frameDue)
        return;

    lastOutfitPreviewFrameTime = now;
    renderer.ShowAnimationFrame(selectedOutfitPreview.animation, 0, outfitPreviewFrame, 0, 32);
    dirtyOutfitPreview = false;

    ++outfitPreviewFrame;
    if (outfitPreviewFrame > selectedOutfitPreview.frameCount)
        outfitPreviewFrame = 1;
}

bool AppearanceSelectionController::loadSelectedOutfitPreview()
{
    hasSelectedOutfitPreview = false;
    selectedOutfitPreview = {};
    outfitPreviewFrame = 1;
    if (selectedOutfitIndex >= outfitOptionCount)
        return false;

    hasSelectedOutfitPreview = appearanceLoader.findOutfitPreview(
        speciesSlot, outfitOptions[selectedOutfitIndex], selectedOutfitPreview);
    if (hasSelectedOutfitPreview)
    {
        selectedOutfitPreview.frameCount = renderer.frameCountFor(selectedOutfitPreview.animation);
        outfitPreviewInterval = renderer.frameIntervalFor(
            selectedOutfitPreview.animation, 0, frameIntervalSlow);
        hasSelectedOutfitPreview = selectedOutfitPreview.frameCount > 0;
    }
    else
    {
        outfitPreviewInterval = frameIntervalSlow;
    }
    dirtyOutfitPreview = true;
    return hasSelectedOutfitPreview;
}

bool AppearanceSelectionController::loadSelectedSpeciesPreview()
{
    hasSelectedOutfitPreview = false;
    selectedOutfitPreview = {};
    outfitPreviewFrame = 1;
    if (selectedSpeciesIndex >= speciesOptionCount || speciesDefaultOutfits[selectedSpeciesIndex] == 0)
        return false;

    hasSelectedOutfitPreview = appearanceLoader.findOutfitPreview(
        speciesOptions[selectedSpeciesIndex],
        speciesDefaultOutfits[selectedSpeciesIndex],
        selectedOutfitPreview);
    if (hasSelectedOutfitPreview)
    {
        selectedOutfitPreview.frameCount = renderer.frameCountFor(selectedOutfitPreview.animation);
        outfitPreviewInterval = renderer.frameIntervalFor(
            selectedOutfitPreview.animation, 0, frameIntervalSlow);
        hasSelectedOutfitPreview = selectedOutfitPreview.frameCount > 0;
    }
    else
    {
        outfitPreviewInterval = frameIntervalSlow;
    }
    dirtyOutfitPreview = true;
    return hasSelectedOutfitPreview;
}

void AppearanceSelectionController::playSelectedChooseAnimation()
{
#if ENABLE_OUTFIT_CHOOSE_ANIMATION
    if (!hasSelectedOutfitPreview)
        return;

    const unsigned long interval = renderer.frameIntervalFor(
        selectedOutfitPreview.animation, 0, frameIntervalSlow);
    const uint16_t frameCount = renderer.frameCountFor(selectedOutfitPreview.animation);
    for (uint16_t frame = 1; frame <= frameCount; ++frame)
    {
        renderer.ShowAnimationFrame(selectedOutfitPreview.animation, 0, frame, 0, 32);
        if (frame < frameCount)
            delay(interval);
    }
#endif
}

void AppearanceSelectionController::changeSelection(int delta)
{
    if (!isActive())
        return;

    if (selectingSpecies)
    {
        if (speciesOptionCount == 0)
            return;

        if (delta < 0)
            selectedSpeciesIndex = (selectedSpeciesIndex == 0) ? (speciesOptionCount - 1) : (selectedSpeciesIndex - 1);
        else
            selectedSpeciesIndex = (selectedSpeciesIndex + 1) % speciesOptionCount;

        loadSelectedSpeciesPreview();
        lastOutfitPreviewFrameTime = 0;
        return;
    }

    if (outfitOptionCount == 0)
        return;

    if (delta < 0)
        selectedOutfitIndex = (selectedOutfitIndex == 0) ? (outfitOptionCount - 1) : (selectedOutfitIndex - 1);
    else
        selectedOutfitIndex = (selectedOutfitIndex + 1) % outfitOptionCount;

    loadSelectedOutfitPreview();
    lastOutfitPreviewFrameTime = 0;
}
