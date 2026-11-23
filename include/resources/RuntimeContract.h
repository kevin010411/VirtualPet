#pragma once

#include "animation/RuntimePlaybackConfig.h"
#include "controller/ButtonBindings.h"
#include "controller/StatusSetContract.h"
#include "display/ScreenLayoutConfig.h"
#include "pet/PetBehaviorTypes.h"
#include "resources/RuntimeTableConfig.h"
#if ENABLE_INSTRUCTION_RESPONSE
#include "game/InstructionRuntimeSettings.h"
#endif
#if ENABLE_STAT_EVENTS
#include "pet/StatEventSettings.h"
#endif

// The loader populates one set of feature configurations for the active appearance.
struct RuntimeContract
{
    PetBehaviorConfig behavior;
    RuntimeTableConfig table;
    ScreenLayoutConfig layout;
    RuntimePlaybackConfig playback;
    StatusSetsConfig statusSets;
    ButtonBindings buttons;
    // Slot 0 is default idle; the remaining slots match PET_STATES in order.
    uint32_t idleButtonOffsets[kMaxRuntimePetStates + 1];
#if ENABLE_INSTRUCTION_RESPONSE
    InstructionRuntimeSettings instruction;
#endif
#if ENABLE_STAT_EVENTS
    StatEventSettings statEvents;
#endif
    uint32_t schemaFingerprint;
    uint8_t activeSpeciesSlot;
    uint8_t activeOutfitSlot;
#if ENABLE_CAPTURE_GAME
    uint8_t captureArtworkSpeciesSlot;
#endif
#if ENABLE_SHOP
    bool shopEnabled;
#endif
};
