#include "app/AppearanceSelectionController.h"

#include <string.h>
#include "app/AppProfile.h"
#include "render/Renderer.h"

AppearanceSelectionController::AppearanceSelectionController(Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef)
{
}

bool AppearanceSelectionController::start(const char *sourceSpeciesCode, const char *currentOutfitCode)
{
    selectingSpecies = false;
    strncpy(speciesCode, sourceSpeciesCode, sizeof(speciesCode) - 1);
    speciesCode[sizeof(speciesCode) - 1] = '\0';
    outfitOptionCount = 0;
    selectedOutfitIndex = 0;
    hasSelectedOutfitPreview = false;

    if (!appearanceLoader.loadOutfits(speciesCode, outfitOptions, maxOutfitOptions, outfitOptionCount))
        return false;

    for (size_t i = 0; i < outfitOptionCount; ++i)
    {
        if (strcmp(outfitOptions[i], currentOutfitCode) == 0)
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

bool AppearanceSelectionController::startSpecies(const char *currentSpeciesCode)
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
        speciesDefaultOutfits[i][0] = '\0';
        appearanceLoader.loadOutfits(speciesOptions[i], &speciesDefaultOutfits[i], 1, defaultOutfitCount);
        if (strcmp(speciesOptions[i], currentSpeciesCode) == 0)
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

bool AppearanceSelectionController::onConfirm(char *selectedOutfit, size_t selectedOutfitSize)
{
    if (selectedOutfit == nullptr || selectedOutfitSize == 0 || outfitOptionCount == 0 || selectedOutfitIndex >= outfitOptionCount)
    {
        exit();
        return false;
    }

    strncpy(selectedOutfit, outfitOptions[selectedOutfitIndex], selectedOutfitSize - 1);
    selectedOutfit[selectedOutfitSize - 1] = '\0';
    playSelectedChooseAnimation();
    exit();
    return true;
}

bool AppearanceSelectionController::onConfirmSpecies(char *selectedSpecies, size_t selectedSpeciesSize, char *selectedOutfit, size_t selectedOutfitSize)
{
    if (selectedSpecies == nullptr || selectedSpeciesSize == 0 ||
        selectedOutfit == nullptr || selectedOutfitSize == 0 ||
        speciesOptionCount == 0 || selectedSpeciesIndex >= speciesOptionCount ||
        speciesDefaultOutfits[selectedSpeciesIndex][0] == '\0')
    {
        exit();
        return false;
    }

    strncpy(selectedSpecies, speciesOptions[selectedSpeciesIndex], selectedSpeciesSize - 1);
    selectedSpecies[selectedSpeciesSize - 1] = '\0';
    strncpy(selectedOutfit, speciesDefaultOutfits[selectedSpeciesIndex], selectedOutfitSize - 1);
    selectedOutfit[selectedOutfitSize - 1] = '\0';
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

void AppearanceSelectionController::render(unsigned long now)
{
    if (!hasSelectedOutfitPreview)
        return;

    const bool frameDue = dirtyOutfitPreview || lastOutfitPreviewFrameTime == 0 || now - lastOutfitPreviewFrameTime >= outfitPreviewInterval;
    if (!frameDue)
        return;

    lastOutfitPreviewFrameTime = now;
    renderer.ShowSDCardFrame(selectedOutfitPreview.path, outfitPreviewFrame, 0, 32);
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

    hasSelectedOutfitPreview = appearanceLoader.findOutfitPreview(speciesCode, outfitOptions[selectedOutfitIndex], selectedOutfitPreview);
    if (hasSelectedOutfitPreview)
    {
        outfitPreviewInterval = selectedOutfitPreview.frameIntervalMs == 0
                                    ? frameIntervalSlow
                                    : max(1UL, static_cast<unsigned long>(selectedOutfitPreview.frameIntervalMs));
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
    if (selectedSpeciesIndex >= speciesOptionCount || speciesDefaultOutfits[selectedSpeciesIndex][0] == '\0')
        return false;

    hasSelectedOutfitPreview = appearanceLoader.findOutfitPreview(
        speciesOptions[selectedSpeciesIndex],
        speciesDefaultOutfits[selectedSpeciesIndex],
        selectedOutfitPreview);
    if (hasSelectedOutfitPreview)
    {
        outfitPreviewInterval = selectedOutfitPreview.frameIntervalMs == 0
                                    ? frameIntervalSlow
                                    : max(1UL, static_cast<unsigned long>(selectedOutfitPreview.frameIntervalMs));
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

    const size_t outfitLen = strlen(selectedOutfitPreview.outfitCode);
    if (outfitLen == 0 || outfitLen + 1 >= sizeof(selectedOutfitPreview.outfitCode))
        return;

    char chooseOutfitCode[9] = {};
    strncpy(chooseOutfitCode, selectedOutfitPreview.outfitCode, sizeof(chooseOutfitCode) - 2);
    chooseOutfitCode[sizeof(chooseOutfitCode) - 2] = '\0';
    strcat(chooseOutfitCode, "c");

    const char *previewSpeciesCode = selectingSpecies ? speciesOptions[selectedSpeciesIndex] : speciesCode;
    OutfitPreview choosePreview = {};
    if (!appearanceLoader.findOutfitPreview(previewSpeciesCode, chooseOutfitCode, choosePreview))
        return;

    const unsigned long interval = choosePreview.frameIntervalMs == 0
                                       ? frameIntervalSlow
                                       : max(1UL, static_cast<unsigned long>(choosePreview.frameIntervalMs));
    for (uint16_t frame = 1; frame <= choosePreview.frameCount; ++frame)
    {
        renderer.ShowSDCardFrame(choosePreview.path, frame, 0, 32);
        if (frame < choosePreview.frameCount)
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
