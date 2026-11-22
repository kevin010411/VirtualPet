#include "resources/AssetResources.h"
#include "display/LayoutRenderer.h"

#include "controller/CommandController.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorTypes.h"
#include "display/Renderer.h"

namespace
{
uint16_t selectStatSumFrame(const ScreenBlockConfig &block, const ScreenRuleConfig *rules,
                            const RuntimeValueContext &values)
{
    const bool evenSegments = block.kind == ScreenBlockKind::StatSumSegments;
    int64_t selector = 0;
    if (block.source != kUnboundScreenSource && !resolveRuntimeValue(block.source, values, selector))
        return block.fallbackFrame;
    for (uint8_t child = 0; child < block.ruleCount; ++child)
    {
        const ScreenRuleConfig &bank = rules[block.firstRule + child];
        if (!evenSegments && selector != bank.minimum)
            continue;
        uint64_t sum = 0;
        for (uint8_t slot = 0; slot < kRuntimeValuePetStatCapacity; ++slot)
            if ((block.numericSettings & (1U << slot)) != 0)
                sum += values.petStats[slot];
        const uint32_t capped = sum > bank.maximum ? bank.maximum : static_cast<uint32_t>(sum);
        // Even segments store the segment count in the first rule field.
        const uint32_t index = evenSegments
            ? static_cast<uint32_t>(static_cast<uint64_t>(capped) * bank.minimum / (static_cast<uint64_t>(bank.maximum) + 1))
            : capped / (block.numericSettings >> 16);
        return static_cast<uint16_t>(bank.frame + index);
    }
    return block.fallbackFrame;
}

uint16_t selectStatDimensionsFrame(const ScreenBlockConfig &block,
                                   const ScreenRuleConfig *rules, const PetBehaviorStatConfig *stats,
                                   const RuntimeValueContext &values)
{
    uint16_t index = 0;
    // The low chunk is the first authored axis and changes slowest.
    for (uint32_t axes = block.numericSettings; (axes & 0xFU) != 0; axes >>= 9)
    {
        const uint8_t slot = static_cast<uint8_t>((axes & 0xFU) - 1);
        const uint8_t levels = static_cast<uint8_t>(((axes >> 4) & 0x1FU) + 1);
        const auto &stat = stats[slot];
        const uint8_t level = quantizeRuntimeValue(values.petStats[slot], stat.minValue, stat.maxValue, levels);
        index = static_cast<uint16_t>(index * levels + level);
    }
    return static_cast<uint16_t>(rules[block.firstRule].frame + index);
}
} // namespace

LayoutRenderer::LayoutRenderer(Renderer &rendererRef, AssetResources &resourcesRef, CommandController &commandsRef)
    : renderer(rendererRef), resources(resourcesRef), commands(commandsRef)
{
}

void LayoutRenderer::configureRuntimeContract(const ScreenLayoutConfig &config, const PetBehaviorConfig &behavior,
                                             const AssetData::BundleId &bundleId)
{
    // Screen blocks, rules and shared layout products belong to the bundle,
    // not the species. Preserve displayed values until syncPlayback compares
    // the next animation's layout ID and current Pet Stats.
    const bool retainLayout = layout != nullptr &&
        AssetData::sameBundleId(layoutBundleId, bundleId);
    layoutBundleId = bundleId;
    layout = &config;
    stats = behavior.stats;
    activeStatMask = 0;
    for (uint8_t slot = 0; slot < behavior.statCount && slot < PetStatSnapshot::kCustomStatCount; ++slot)
        activeStatMask |= static_cast<uint16_t>(1U << slot);
    restoreAnimationArea();
    for (uint8_t index = 0; index < config.screenBlockCount; ++index)
    {
        if (!retainLayout)
            numericFrames[index] = config.screenBlocks[index].fallbackFrame;
    }
    if (!retainLayout)
        hasActiveLayout = false;
}

void LayoutRenderer::restoreAnimationArea()
{
    renderer.setAnimationArea(0, 0, 0, 0);
    if (layout == nullptr) return;
    for (uint8_t index = 0; index < layout->screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = layout->screenBlocks[index];
        if (block.kind == ScreenBlockKind::Animation)
            renderer.setAnimationArea(block.x, block.y, block.width, block.height);
    }
}

void LayoutRenderer::begin()
{
    restoreAnimationArea();
    hasActiveLayout = false;
}

void LayoutRenderer::drawAll()
{
    if (!hasActiveLayout || layout == nullptr)
        return;
    if (!renderer.ShowAnimationFrame(layout->layoutUnselected, activeLayoutId,
                                     1, 0, 0, 12, 128, 160))
        return;
    const int selectedSlot = commands.selectedSlot();

    for (int slot = 0; slot < commands.commandCount(); ++slot)
    {
        if (!drawSlot(slot, slot == selectedSlot && commands.isSlotVisible(slot)))
            return;
    }
    for (uint8_t index = 0; index < layout->screenBlockCount; ++index)
        if (layout->screenBlocks[index].isNumeric() && !drawNumeric(index))
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
    bool fullScreen = false;
    if (!renderer.currentLayoutId(layoutId, &fullScreen))
    {
        resources.recordAssetDataErrorResource("asset data");
        return false;
    }
    if (fullScreen)
        hasActiveLayout = false;
    updateValues(snapshot);
    if (fullScreen)
        return true;
    return updatePlayback(layoutId);
}

bool LayoutRenderer::updatePlayback(uint8_t layoutId)
{
    if (layout == nullptr)
        return false;
    if (hasActiveLayout && activeLayoutId == layoutId)
        return true;
    activeLayoutId = layoutId;
    hasActiveLayout = true;
    drawAll();
    return resources.firstAssetDataError() == AssetData::BundleError::None;
}

bool LayoutRenderer::drawSlot(int slot, bool selected)
{
    if (!commandsVisible) return true;
    if (layout == nullptr)
        return false;
    if (!hasActiveLayout)
        return false;
    const AssetData::AnimationRef &layoutAnimation = selected
                                                ? layout->layoutSelected
                                                : layout->layoutUnselected;
    for (uint8_t index = 0; index < layout->screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = layout->screenBlocks[index];
        if (block.kind != ScreenBlockKind::Button || block.source != slot + 1)
            continue;
        if (!renderer.ShowAnimationFrame(layoutAnimation, activeLayoutId, index + 2,
                                         block.x, block.y, 12, block.width, block.height))
            return false;
    }
    return true;
}

void LayoutRenderer::updateValues(const PetStatSnapshot &snapshot)
{
    if (layout == nullptr)
        return;
    const auto values = makeRuntimeValueContext(snapshot.customStats, activeStatMask, snapshot.stage_days);
    for (uint8_t index = 0; index < layout->screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = layout->screenBlocks[index];
        if (!block.isNumeric())
            continue;
        uint16_t frame = block.fallbackFrame;
        int64_t value = 0;
        if (block.kind == ScreenBlockKind::StatSum || block.kind == ScreenBlockKind::StatSumSegments)
        {
            frame = selectStatSumFrame(block, layout->screenRules, values);
        }
        else if (block.kind == ScreenBlockKind::StatDimensions)
        {
            frame = selectStatDimensionsFrame(block, layout->screenRules, stats, values);
        }
        else if (block.source != kUnboundScreenSource && resolveRuntimeValue(block.source, values, value))
        {
            for (uint8_t child = 0; child < block.ruleCount; ++child)
            {
                const ScreenRuleConfig &rule = layout->screenRules[block.firstRule + child];
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
    const ScreenBlockConfig &block = layout->screenBlocks[index];
    return renderer.ShowAnimationFrame(layout->layoutUnselected, activeLayoutId,
                                       numericFrames[index], block.x, block.y, 12,
                                       block.width, block.height);
}
