#include "controller/ProjectMenuSession.h"

#if ENABLE_PROJECT_MENUS
#include "display/Renderer.h"
#include "resources/AssetResources.h"
#include "resources/detail/RuntimeTableFile.h"

using namespace RuntimeTableInternal;

namespace
{
constexpr uint8_t rowHeight = 16;
constexpr uint8_t rowsPerPage = 10;

bool readDefinition(const RuntimeTable &table, uint16_t menu, uint8_t (&record)[4])
{
    const Section *definitions = table.find(MenuDefinitions);
    return definitions && readRecord(table.source, *definitions, menu, record);
}
}

ProjectMenuSession::ProjectMenuSession(Renderer &rendererRef, AssetResources &resourcesRef,
                                       const AssetData::RuntimeManifest &manifestRef)
    : renderer(rendererRef), resources(resourcesRef), manifest(manifestRef)
{
}

void ProjectMenuSession::close()
{
    depth = 0;
    card = kNone16;
}

bool ProjectMenuSession::pop()
{
    if (card != kNone16)
        card = kNone16;
    else
        --depth;
    return active();
}

bool ProjectMenuSession::back()
{
    return !pop() || redraw();
}

bool ProjectMenuSession::open(uint8_t menu)
{
    close();
    stack[depth++] = {menu, 0};
    return redraw();
}

bool ProjectMenuSession::move(bool forward)
{
    if (card != kNone16) return true;
    RuntimeTableFile file;
    if (!file.open(resources.sdCard(), manifest)) return false;
    Cursor &cursor = stack[depth - 1];
    uint8_t definition[4];
    if (!readDefinition(file.table(), cursor.menu, definition)) return false;
    const uint16_t count = readU16(definition + 2);
    if (count == 0) return false;
    cursor.position = forward ? (cursor.position + 1) % count :
        (cursor.position == 0 ? count - 1 : cursor.position - 1);
    return redraw(file.table());
}

bool ProjectMenuSession::confirm(Entry &entry)
{
    RuntimeTableFile file;
    if (!file.open(resources.sdCard(), manifest)) return false;
    const RuntimeTable &table = file.table();
    if (card != kNone16)
    {
        pop();
        return redraw(table);
    }
    const Cursor &cursor = stack[depth - 1];
    uint8_t definition[4];
    const Section *entries = table.find(MenuEntries);
    uint8_t row[8];
    if (!entries || !readDefinition(table, cursor.menu, definition) ||
        !readRecord(table.source, *entries, readU16(definition) + cursor.position, row))
        return false;

    const Kind kind = static_cast<Kind>(row[0]);
    const uint16_t target = readU16(row + 2);
    switch (kind)
    {
    case Kind::Back:
        return !pop() || redraw(table);
    case Kind::Menu:
        if (depth == sizeof(stack) / sizeof(stack[0])) return false;
        stack[depth++] = {target, 0};
        return redraw(table);
    case Kind::TextCard:
        card = target;
        return redraw(table);
    case Kind::UserAction:
    case Kind::SystemCommand:
        entry = {kind, target};
        break;
    case Kind::None:
        break;
    }
    return true;
}

bool ProjectMenuSession::redraw()
{
    RuntimeTableFile file;
    return file.open(resources.sdCard(), manifest) && redraw(file.table());
}

bool ProjectMenuSession::redraw(const RuntimeTable &table)
{
    if (card != kNone16) return show(table, card, 0, 0, 160);
    const Cursor &cursor = stack[depth - 1];
    uint8_t definition[4];
    const Section *entries = table.find(MenuEntries);
    if (!entries || !readDefinition(table, cursor.menu, definition)) return false;
    const uint16_t first = readU16(definition);
    const uint16_t count = readU16(definition + 2);
    const uint16_t page = cursor.position - cursor.position % rowsPerPage;
    uint8_t y = 0;
    for (uint16_t position = page; position < count && position < page + rowsPerPage; ++position)
    {
        uint8_t row[8];
        if (!readRecord(table.source, *entries, first + position, row) ||
            !show(table, readU16(row + 4), readU16(row + 6) + (position == cursor.position),
                  y, rowHeight))
            return false;
        y += rowHeight;
    }
    if (y < 160) renderer.clearMenuBelow(y);
    return true;
}

bool ProjectMenuSession::show(const RuntimeTable &table, uint16_t asset, uint16_t frame,
                              uint8_t y, uint8_t height)
{
    const Section *assets = table.find(AssetRefs);
    AssetData::AnimationRef animation;
    return assets && resolveAssetReference(table.source, *assets, asset, animation) &&
        renderer.ShowAnimationFrame(animation, 0, frame + 1, 0, y, 12, 128, height);
}
#endif
