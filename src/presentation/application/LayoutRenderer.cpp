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
    if (!hasActiveLayout || runtimeContract == nullptr)
        return;
    if (!renderer.ShowAnimationFrame(runtimeContract->layoutUnselected, activeLayoutId,
                                     1, 0, 0, 12, 128, 160))
        return;
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
                                        runtimeContract->layoutSelected, layoutId,
                                        runtimeContract->screenBlockCount))
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
        if (!renderer.ShowAnimationFrame(layout, activeLayoutId, index + 2,
                                         block.x, block.y, 12, block.width, block.height))
            return false;
    }
    return true;
}
