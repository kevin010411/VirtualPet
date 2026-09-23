#include "presentation/application/LayoutRenderer.h"

#include "commands/application/CommandController.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"
#include "presentation/adapters/rendering/Renderer.h"

LayoutRenderer::LayoutRenderer(Renderer &rendererRef, CommandController &commandsRef)
    : renderer(rendererRef), commands(commandsRef)
{
}

void LayoutRenderer::configureRuntimeContract(const PetBehaviorConfig &config)
{
    runtimeContract = &config;
    hasActiveLayout = false;
}

void LayoutRenderer::begin()
{
    hasActiveLayout = false;
}

void LayoutRenderer::drawAll()
{
    const int selectedSlot = commands.selectedSlot();

    for (int slot = 0; slot < commands.commandCount(); ++slot)
    {
        if (!drawSlot(slot, slot == selectedSlot))
            return;
    }
}

void LayoutRenderer::drawSelection()
{
    const int prevIdx = commands.previousSlot();
    if (commands.isSlotVisible(prevIdx))
        drawSlot(prevIdx, false);

    const int curIdx = commands.selectedSlot();
    if (commands.isSlotVisible(curIdx))
        drawSlot(curIdx, true);
}

bool LayoutRenderer::updatePlayback(uint8_t layoutId)
{
    if (runtimeContract == nullptr)
        return false;
    if (hasActiveLayout && activeLayoutId == layoutId)
        return true;
    if (!renderer.validateLayoutVersion(runtimeContract->layoutUnselected,
                                        runtimeContract->layoutSelected, layoutId))
        return false;
    activeLayoutId = layoutId;
    hasActiveLayout = true;
    drawAll();
    return renderer.firstAssetDataError() == AssetData::BundleError::None;
}

bool LayoutRenderer::drawSlot(int slot, bool selected)
{
    if (runtimeContract == nullptr)
        return false;
    if (!hasActiveLayout)
        return false;
    const AssetData::AnimationRef &layout = selected
                                                ? runtimeContract->layoutSelected
                                                : runtimeContract->layoutUnselected;
    return renderer.ShowAnimationFrame(
        layout,
        activeLayoutId,
        static_cast<uint16_t>(slot + 1),
        slotX(slot),
        slotY(slot));
}

int LayoutRenderer::slotX(int slot)
{
    return (slot % 4) * tileSize;
}

int LayoutRenderer::slotY(int slot)
{
    return (slot < 4) ? 0 : (screenHeight - tileSize);
}
