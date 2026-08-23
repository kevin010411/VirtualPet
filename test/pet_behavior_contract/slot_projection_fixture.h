#ifndef SLOT_PROJECTION_FIXTURE_H
#define SLOT_PROJECTION_FIXTURE_H

#include <stdint.h>

// Generated from the authoritative Web fixture directory:
// E:\C++\virtualPet\web\tests\fixtures\pet_stat_slot_projection
// The saved Web identity is custom4; runtime records expose only custom0.
constexpr uint32_t kSlotProjectionSchemaFingerprint = 0xE1719630UL;
constexpr const char *kSlotProjectionFixture =
    "runtime_contract|1\n"
    "pet_behavior|3|E1719630\n"
    "status|1\n"
    "stat|custom0|100|0|100|-2\n"
    "idle|anim0\n"
    "idle_trigger|trigger0|custom0|<|20|anim5\n"
    "action|action0|anim6|1\n"
    "action_effect|effect0|action0|change|custom0|7\n"
    "guess_effect|guess_effect0|round_correct|set|custom0|88\n"
    "button|1|user_action|action0\n"
    "button|2|empty|\n"
    "button|3|empty|\n"
    "button|4|empty|\n"
    "button|5|empty|\n"
    "button|6|empty|\n"
    "button|7|empty|\n"
    "button|8|system_command|status\n"
    "status_set|set0|StatusCustom4\n"
    "status_condition|set0|condition0|custom0|2|0|100\n"
    "crc32|D44082F8\n";

constexpr const char *kSlotProjectionEvolutionFixture =
    "# source species|target species|target outfit|conditions\n"
    "init|pet01|base|\n"
    "pet01|pet02|base|custom0=50..*\n";

constexpr const char *kSlotProjectionEvolutionPetStatSource = "custom0";
constexpr const char *kSlotProjectionEvolutionConditions = "custom0=50..*";

#endif // SLOT_PROJECTION_FIXTURE_H
