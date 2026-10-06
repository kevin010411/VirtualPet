#include "appearance/AppearanceSelectionController.h"

#include "display/Renderer.h"
#include "display/LayoutRenderer.h"

AppearanceSelectionController::AppearanceSelectionController(Renderer &rendererRef,
    AppearanceLoader &appearanceLoaderRef, LayoutRenderer &layoutRef)
    : renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef), layout(layoutRef)
{
}

bool AppearanceSelectionController::start(uint8_t sourceSpeciesSlot, uint8_t currentOutfitSlot,
                                          uint8_t sourceUnlockMask)
{
    speciesSlot = sourceSpeciesSlot;
    unlockMask = sourceUnlockMask;
    outfitOptionCount = 0;
    selectedOutfitIndex = 0;
    hasSelectedOutfitPreview = false;

    if (!appearanceLoader.loadOutfits(speciesSlot, unlockMask, outfitOptions, maxOutfitOptions, outfitOptionCount))
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

bool AppearanceSelectionController::isActive() const
{
    return selectingOutfit;
}

void AppearanceSelectionController::onLeft()
{
    changeSelection(-1);
}

void AppearanceSelectionController::onRight()
{
    changeSelection(1);
}

bool AppearanceSelectionController::onConfirm(uint8_t &selectedOutfitSlot, bool &requiresUnlock)
{
    requiresUnlock = false;
    if (outfitOptionCount == 0 || selectedOutfitIndex >= outfitOptionCount)
    {
        exit();
        return false;
    }

    selectedOutfitSlot = outfitOptions[selectedOutfitIndex];
    requiresUnlock = (unlockMask & (1U << (selectedOutfitSlot - 1U))) == 0;
    if (requiresUnlock)
        return true;
    exit();
    return true;
}

void AppearanceSelectionController::exit()
{
    selectingOutfit = false;
    outfitOptionCount = 0;
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
    layout.begin();
    dirtyOutfitPreview = true;
    lastOutfitPreviewFrameTime = 0;
}

bool AppearanceSelectionController::preparePreviewLayout()
{
    uint8_t layoutId = 0;
    return renderer.setAnimation(selectedOutfitPreview.animation, 0, false) &&
           renderer.currentLayoutId(layoutId) && layout.updatePlayback(layoutId);
}

void AppearanceSelectionController::render(unsigned long now)
{
    if (!hasSelectedOutfitPreview)
        return;

    const bool frameDue = dirtyOutfitPreview || lastOutfitPreviewFrameTime == 0 || now - lastOutfitPreviewFrameTime >= outfitPreviewInterval;
    if (!frameDue)
        return;

    if ((dirtyOutfitPreview || lastOutfitPreviewFrameTime == 0) && !preparePreviewLayout())
        return;

    lastOutfitPreviewFrameTime = now;
    renderer.ShowAnimationFrame(selectedOutfitPreview.animation, 0, outfitPreviewFrame);
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

    const uint8_t selectedSlot = outfitOptions[selectedOutfitIndex];
    hasSelectedOutfitPreview = appearanceLoader.findOutfitPreview(
        speciesSlot, selectedSlot,
        (unlockMask & (1U << (selectedSlot - 1U))) == 0, selectedOutfitPreview);
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

void AppearanceSelectionController::changeSelection(int delta)
{
    if (!isActive())
        return;

    if (outfitOptionCount == 0)
        return;

    if (delta < 0)
        selectedOutfitIndex = (selectedOutfitIndex == 0) ? (outfitOptionCount - 1) : (selectedOutfitIndex - 1);
    else
        selectedOutfitIndex = (selectedOutfitIndex + 1) % outfitOptionCount;

    loadSelectedOutfitPreview();
    lastOutfitPreviewFrameTime = 0;
}
