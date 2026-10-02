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
        if (!drawSlot(slot, slot == selectedSlot && commands.isSlotVisible(slot)))
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
    for (uint8_t index = 0; index < runtimeContract->screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = runtimeContract->screenBlocks[index];
        if (block.kind != ScreenBlockKind::Button || block.source != slot + 1)
            continue;
        if (!renderer.ShowAnimationFrame(layout, activeLayoutId, block.source,
                                         block.x, block.y))
            return false;
    }
    return true;
}
