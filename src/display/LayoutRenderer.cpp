#include "display/LayoutRenderer.h"

#include "controller/CommandController.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorTypes.h"
#include "display/Renderer.h"

LayoutRenderer::LayoutRenderer(Renderer &rendererRef, CommandController &commandsRef)
    : renderer(rendererRef), commands(commandsRef)
{
}

void LayoutRenderer::configureRuntimeContract(const PetBehaviorConfig &config)
{
    runtimeContract = &config;
    renderer.setAnimationArea(0, 0, 0, 0);
    for (uint8_t index = 0; index < config.screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = config.screenBlocks[index];
        if (block.kind == ScreenBlockKind::Animation)
            renderer.setAnimationArea(block.x, block.y, block.width, block.height);
        numericFrames[index] = config.screenBlocks[index].fallbackFrame;
    }
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
    for (uint8_t index = 0; index < runtimeContract->screenBlockCount; ++index)
        if (runtimeContract->screenBlocks[index].kind == ScreenBlockKind::Stat && !drawNumeric(index))
            return;
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

bool LayoutRenderer::syncPlayback(const PetStatSnapshot &snapshot)
{
    uint8_t layoutId = 0;
    if (!renderer.currentLayoutId(layoutId))
    {
        renderer.recordAssetDataErrorResource("asset data");
        return false;
    }
    updateValues(snapshot);
    return updatePlayback(layoutId);
}

bool LayoutRenderer::updatePlayback(uint8_t layoutId)
{
    if (runtimeContract == nullptr)
        return false;
    if (hasActiveLayout && activeLayoutId == layoutId)
        return true;
    if (!renderer.validateLayoutVersion(runtimeContract->layoutUnselected,
                                        runtimeContract->layoutSelected, layoutId,
                                        runtimeContract->screenBlockCount, runtimeContract->screenProductFrameCount))
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

void LayoutRenderer::updateValues(const PetStatSnapshot &snapshot)
{
    if (runtimeContract == nullptr)
        return;
    RuntimeValueContext values = {};
    values.petStats = snapshot.customStats;
    values.stageDays = snapshot.stage_days;
    for (uint8_t slot = 0; slot < runtimeContract->statCount && slot < PetStatSnapshot::kCustomStatCount; ++slot)
        values.activePetStatMask |= static_cast<uint16_t>(1U << slot);
    for (uint8_t index = 0; index < runtimeContract->screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = runtimeContract->screenBlocks[index];
        if (block.kind != ScreenBlockKind::Stat)
            continue;
        uint16_t frame = block.fallbackFrame;
        int32_t value = 0;
        if (block.source != kUnboundScreenSource && resolveRuntimeValue(block.source, values, value))
        {
            for (uint8_t child = 0; child < block.ruleCount; ++child)
            {
                const ScreenRuleConfig &rule = runtimeContract->screenRules[block.firstRule + child];
                if (value >= rule.minimum && value <= rule.maximum)
                {
                    frame = rule.frame;
                    break;
                }
            }
        }
        if (numericFrames[index] == frame)
            continue;
        numericFrames[index] = frame;
        if (hasActiveLayout && !drawNumeric(index))
            return;
    }
}

bool LayoutRenderer::drawNumeric(uint8_t index)
{
    const ScreenBlockConfig &block = runtimeContract->screenBlocks[index];
    return renderer.ShowAnimationFrame(runtimeContract->layoutUnselected, activeLayoutId,
                                       numericFrames[index], block.x, block.y, 12,
                                       block.width, block.height);
}
