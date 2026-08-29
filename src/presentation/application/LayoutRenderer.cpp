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
}

void LayoutRenderer::begin()
{
    actionMode = false;
    activeAction = FirmwarePlaybackRole::None;
    activeActionSlot = -1;

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
#if ENABLE_DYNAMIC_ACTION_LAYOUT
    if (actionMode)
    {
        drawAll();
        return;
    }
#endif

    const int prevIdx = commands.previousSlot();
    if (commands.isSlotVisible(prevIdx))
        drawSlot(prevIdx, false);

    const int curIdx = commands.selectedSlot();
    if (commands.isSlotVisible(curIdx))
        drawSlot(curIdx, true);
}

bool LayoutRenderer::enterAction(FirmwarePlaybackRole id, int activeSlot)
{
#if ENABLE_DYNAMIC_ACTION_LAYOUT
    actionMode = true;
    activeAction = hasActionLayout(id) ? id : FirmwarePlaybackRole::None;
    activeActionSlot = activeSlot;
    drawAll();
    return true;
#else
    (void)id;
    (void)activeSlot;
    return false;
#endif
}

bool LayoutRenderer::updateAction(FirmwarePlaybackRole id)
{
#if ENABLE_DYNAMIC_ACTION_LAYOUT
    if (!actionMode)
        return false;

    const FirmwarePlaybackRole nextAction = hasActionLayout(id) ? id : FirmwarePlaybackRole::None;
    if (activeAction == nextAction)
        return false;

    activeAction = nextAction;
    drawAll();
    return true;
#else
    (void)id;
    return false;
#endif
}

bool LayoutRenderer::endAction()
{
    if (!actionMode)
        return false;

    actionMode = false;
    activeAction = FirmwarePlaybackRole::None;
    activeActionSlot = -1;
    drawAll();
    return true;
}

bool LayoutRenderer::isActionActive() const
{
    return actionMode;
}

bool LayoutRenderer::drawSlot(int slot, bool selected)
{
    if (runtimeContract == nullptr)
        return false;
    const AssetData::AnimationRef &layout = selected
                                                ? runtimeContract->layoutSelected
                                                : runtimeContract->layoutUnselected;
    return renderer.ShowAnimationFrame(
        layout,
        layoutVersion(activeAction),
        static_cast<uint16_t>(slot + 1),
        slotX(slot),
        slotY(slot));
}

bool LayoutRenderer::hasActionLayout(FirmwarePlaybackRole id) const
{
    return layoutVersion(id) != 0;
}

uint8_t LayoutRenderer::layoutVersion(FirmwarePlaybackRole id) const
{
    if (runtimeContract == nullptr)
        return 0;
    const size_t index = static_cast<size_t>(id);
    if (index >= kFirmwarePlaybackRoleCount)
        return 0;
    return runtimeContract->actionLayoutVersions[index];
}

int LayoutRenderer::slotX(int slot)
{
    return (slot % 4) * tileSize;
}

int LayoutRenderer::slotY(int slot)
{
    return (slot < 4) ? 0 : (screenHeight - tileSize);
}
