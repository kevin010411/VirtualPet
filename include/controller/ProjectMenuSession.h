#pragma once

#include "common/AppProfile.h"
#if ENABLE_PROJECT_MENUS
#include <stdint.h>

class AssetResources;
class Renderer;
struct PetBehaviorConfig;
namespace RuntimeTableInternal
{
struct RuntimeTable;
}

class ProjectMenuSession
{
public:
    enum class Kind : uint8_t { None, UserAction, SystemCommand, Menu, TextCard, Back };
    struct Entry
    {
        Kind kind = Kind::None;
        uint16_t target = 0;
    };

    ProjectMenuSession(Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config);
    bool open(uint8_t menu);
    bool redraw();
    bool move(bool forward);
    bool confirm(Entry &entry);
    bool back();
    bool active() const { return depth != 0; }
    void close();

private:
    struct Cursor
    {
        uint16_t menu;
        uint16_t position;
    };

    bool redraw(const RuntimeTableInternal::RuntimeTable &table);
    bool pop();
    bool show(const RuntimeTableInternal::RuntimeTable &table, uint16_t asset,
              uint16_t frame, uint8_t y, uint8_t height);

    Renderer &renderer;
    AssetResources &resources;
    const PetBehaviorConfig &config;
    Cursor stack[16] = {};
    uint8_t depth = 0;
    uint16_t card = 0xffffU;
};
#endif
