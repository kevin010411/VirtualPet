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
    activeScene = nullptr;
}

void LayoutRenderer::begin()
{
    actionMode = false;
    activeScene = nullptr;
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
    (void)id;
    (void)activeSlot;
    actionMode = true;
    drawAll();
    return true;
#else
    (void)id;
    (void)activeSlot;
    return false;
#endif
}

bool LayoutRenderer::updatePlayback(const AssetData::AnimationRef &animation,
                                    uint8_t versionIndex)
{
    const RuntimeAnimationSceneConfig *nextScene = sceneFor(animation, versionIndex);
    if (activeScene == nextScene)
        return false;
    activeScene = nextScene;
    drawAll();
    return true;
}

bool LayoutRenderer::endAction()
{
    if (!actionMode)
        return false;

    actionMode = false;
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
    if (activeScene == nullptr)
        return false;
    const AssetData::AnimationRef &layout = selected
                                                ? activeScene->selected
                                                : activeScene->unselected;
    return renderer.ShowAnimationFrame(
        layout,
        activeScene->layoutVersion,
        static_cast<uint16_t>(slot + 1),
        slotX(slot),
        slotY(slot));
}

const RuntimeAnimationSceneConfig *LayoutRenderer::sceneFor(
    const AssetData::AnimationRef &animation,
    uint8_t versionIndex) const
{
    if (runtimeContract == nullptr || !animation.valid())
        return nullptr;
    for (uint16_t index = 0; index < runtimeContract->animationSceneCount; ++index)
    {
        const RuntimeAnimationSceneConfig &scene =
            runtimeContract->animationScenes[index];
        if (scene.active && scene.animationVersion == versionIndex &&
            scene.animation.speciesSlot == animation.speciesSlot &&
            scene.animation.outfitSlot == animation.outfitSlot &&
            scene.animation.animationId == animation.animationId)
            return &scene;
    }
    return nullptr;
}

int LayoutRenderer::slotX(int slot)
{
    return (slot % 4) * tileSize;
}

int LayoutRenderer::slotY(int slot)
{
    return (slot < 4) ? 0 : (screenHeight - tileSize);
}
