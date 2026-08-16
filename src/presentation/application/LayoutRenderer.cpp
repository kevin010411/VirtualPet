#include "presentation/application/LayoutRenderer.h"

#include <SdFat.h>
#include <string.h>
#include "shared/config/AppProfile.h"
#include "shared/sd/SdTextRecordReader.h"
#include "shared/utils/TextBuffer.h"
#include "commands/application/CommandController.h"
#include "presentation/adapters/rendering/Renderer.h"

namespace
{
constexpr const char *kLayoutIndexPath = "/layout/index.txt";
constexpr const char *kLegacyLayoutPath = "/layout";
constexpr const char *kLegacySelectedLayoutPath = "/layout_sel";
constexpr const char *kBaseSelectedLayoutPath = "/layout/base_on";
constexpr const char *kBaseLayoutPath = "/layout/base_off";
constexpr size_t kMaxLayoutIndexFileBytes = kAnimationIdCount * 32U;
constexpr size_t kMaxLayoutIndexLineBytes = 32;

void trimLine(char *line)
{
    if (line == nullptr)
        return;

    char *comment = strchr(line, '#');
    if (comment != nullptr)
        *comment = '\0';

    char *start = line;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        ++start;

    if (start != line)
        memmove(line, start, strlen(start) + 1);

    size_t length = strlen(line);
    while (length > 0)
    {
        const char last = line[length - 1];
        if (last != ' ' && last != '\t' && last != '\r' && last != '\n')
            break;

        line[length - 1] = '\0';
        --length;
    }
}

struct ActionLayoutLoadContext
{
    bool layouts[kAnimationIdCount] = {};
};

SdTextRecordAction collectActionLayout(void *rawContext, const SdTextRecord &record)
{
    if (rawContext == nullptr || record.fieldOverflow || record.fieldCount != 1)
        return SdTextRecordAction::Continue;

    char *line = record.fields[0];
    trimLine(line);
    const AnimationId id = animationIdFromName(line);
    if (id != AnimationId::None)
        static_cast<ActionLayoutLoadContext *>(rawContext)->layouts[static_cast<size_t>(id)] = true;
    return SdTextRecordAction::Continue;
}
} // namespace

LayoutRenderer::LayoutRenderer(Renderer &rendererRef, CommandController &commandsRef)
    : renderer(rendererRef), commands(commandsRef)
{
}

void LayoutRenderer::begin()
{
    actionMode = false;
    activeAction = AnimationId::None;
    activeActionSlot = -1;

    for (size_t i = 0; i < kAnimationIdCount; ++i)
        actionLayouts[i] = false;

#if ENABLE_DYNAMIC_ACTION_LAYOUT
    loadActionLayouts();
#endif
}

void LayoutRenderer::drawAll()
{
    const int selectedSlot = commands.selectedSlot();

    for (int slot = 0; slot < commands.commandCount(); ++slot)
        drawSlot(slot, slot == selectedSlot);
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

bool LayoutRenderer::enterAction(AnimationId id, int activeSlot)
{
#if ENABLE_DYNAMIC_ACTION_LAYOUT
    actionMode = true;
    activeAction = hasActionLayout(id) ? id : AnimationId::None;
    activeActionSlot = activeSlot;
    drawAll();
    return true;
#else
    (void)id;
    (void)activeSlot;
    return false;
#endif
}

bool LayoutRenderer::updateAction(AnimationId id)
{
#if ENABLE_DYNAMIC_ACTION_LAYOUT
    if (!actionMode)
        return false;

    const AnimationId nextAction = hasActionLayout(id) ? id : AnimationId::None;
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
    activeAction = AnimationId::None;
    activeActionSlot = -1;
    drawAll();
    return true;
}

bool LayoutRenderer::isActionActive() const
{
    return actionMode;
}

void LayoutRenderer::loadActionLayouts()
{
    SdFat *sd = renderer.sdCard();
    if (sd == nullptr)
        return;

    ActionLayoutLoadContext context;
    if (!loadSdDelimitedTextRecords(sd, kLayoutIndexPath, kMaxLayoutIndexFileBytes,
                                    kMaxLayoutIndexLineBytes, collectActionLayout, &context))
        return;

    memcpy(actionLayouts, context.layouts, sizeof(actionLayouts));
}

void LayoutRenderer::drawSlot(int slot, bool selected)
{
#if ENABLE_DYNAMIC_ACTION_LAYOUT
    if (activeAction != AnimationId::None)
    {
        char path[48];
        TextBuffer layoutPath(path, sizeof(path));
        if (layoutPath.append("/layout/") && layoutPath.append(animationNameFromId(activeAction)) &&
            layoutPath.append("_") && layoutPath.append(selected ? "on" : "off") && layoutPath.ok())
        {
            drawSlotFromPath(slot, path);
            return;
        }
    }

    drawSlotFromPath(slot, selected ? kBaseSelectedLayoutPath : kBaseLayoutPath);
#else
    drawSlotFromPath(slot, selected ? kLegacySelectedLayoutPath : kLegacyLayoutPath);
#endif
}

void LayoutRenderer::drawSlotFromPath(int slot, const char *path)
{
    renderer.ShowSDCardFrame(path, static_cast<uint16_t>(slot + 1), slotX(slot), slotY(slot));
}

bool LayoutRenderer::hasActionLayout(AnimationId id) const
{
    const size_t index = static_cast<size_t>(id);
    return index < kAnimationIdCount && actionLayouts[index];
}

int LayoutRenderer::slotX(int slot)
{
    return (slot % 4) * tileSize;
}

int LayoutRenderer::slotY(int slot)
{
    return (slot < 4) ? 0 : (screenHeight - tileSize);
}
