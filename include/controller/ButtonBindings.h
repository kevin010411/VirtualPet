#pragma once

#include "controller/SystemCommandCatalog.h"

constexpr uint8_t kPetBehaviorButtonCount = 8;

enum class PetBehaviorButtonKind : uint8_t
{
    Empty,
    UserAction,
    SystemCommand,
    Menu,
};

struct PetBehaviorButtonConfig
{
    bool active;
    PetBehaviorButtonKind kind;
    uint8_t actionSlot;
    RuntimeSystemCommandId systemCommandId;
#if ENABLE_PROJECT_MENUS
    uint8_t menuSlot;
#endif
};

struct ButtonBindings
{
    PetBehaviorButtonConfig buttons[kPetBehaviorButtonCount];
    uint8_t buttonCount;
};
