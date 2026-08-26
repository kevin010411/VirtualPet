#include <assert.h>
#include <string.h>

#include "appearance/adapters/EvolutionConditionContract.h"
#include "commands/domain/StatusSetContract.h"
#include "pet/adapters/PetStateSchemaDecision.h"
#include "pet_behavior/domain/PetBehaviorContract.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "shared/sd/SdTextRecordReader.h"
#include "slot_projection_fixture.h"

namespace
{
struct FixtureStatContext
{
    const ActivePetBehaviorStatSlots *activeSlots;
    const PetStatSnapshot *stats;
};

bool fixtureStatusValue(const char *source, const void *rawContext, int32_t &value)
{
    const FixtureStatContext &context = *static_cast<const FixtureStatContext *>(rawContext);
    uint8_t slot = 0;
    if (!context.activeSlots->resolve(source, slot))
        return false;
    value = context.stats->customStats[slot];
    return true;
}

constexpr const char *kMinimalContract =
    "runtime_contract|3\n"
    "pet_behavior|3|00000000\n"
    "status|1\n"
    "status_set|set0|Status\n"
    "idle|anim0\n"
    "button|1|empty|\n"
    "button|2|empty|\n"
    "button|3|empty|\n"
    "button|4|empty|\n"
    "button|5|empty|\n"
    "button|6|empty|\n"
    "button|7|empty|\n"
    "button|8|empty|\n"
    "crc32|EEA13AB1\n";

void assertRejectedAndCleared(const char *contract)
{
    PetBehaviorConfig config = {};
    config.statCount = 7;
    strcpy(config.idleAnimation, "sentinel");
    assert(!parsePetBehaviorContract(contract, config));
    assert(config.statCount == 0);
    assert(config.idleAnimation[0] == '\0');
}

void testMinimalContractParsesCountsAndButtons()
{
    PetBehaviorConfig config = {};
    assert(parsePetBehaviorContract(kMinimalContract, config));
    assert(config.statCount == 0);
    assert(config.idleTriggerCount == 0);
    assert(config.actionCount == 0);
    assert(config.actionEffectCount == 0);
    assert(config.buttonCount == 8);
    assert(strcmp(config.idleAnimation, "anim0") == 0);
    for (uint8_t slot = 0; slot < config.buttonCount; ++slot)
        assert(config.buttons[slot].kind == PetBehaviorButtonKind::Empty);
}

void testSlotsBeyondConfiguredPetStatCapacityAreRejected()
{
    PetBehaviorConfig config = {};
    assert(!parsePetBehaviorContract(
        "runtime_contract|1\n"
        "pet_behavior|3|12345678\n"
        "stat|custom6|5|0|10|-1\n",
        config));
}

#if ENABLE_GUESS_GAME
void testWebExportedSlotProjectionTargetsOneRuntimeSlot()
{
    PetBehaviorConfig config = {};
    assert(parsePetBehaviorContract(kSlotProjectionFixture, config));
    assert(config.schemaFingerprint == kSlotProjectionSchemaFingerprint);
    assert(config.statCount == 1);
    assert(config.stats[0].active);
    assert(config.idleTriggerCount == 1);
    assert(config.idleTriggers[0].statSlot == 0);
    assert(config.actionEffectCount == 1);
    assert(config.actionEffects[0].statSlot == 0);
    assert(config.guessEffectCount == 1);
    assert(config.guessEffects[0].statSlot == 0);
    assert(config.statusSets.count == 1);
    assert(config.statusSets.sets[0].conditionCount == 1);
    assert(strcmp(config.statusSets.sets[0].conditions[0].source, "custom0") == 0);

    ActivePetBehaviorStatSlots activeSlots(config);
    uint8_t statusSlot = 0;
    uint8_t evolutionSlot = 0;
    assert(activeSlots.resolve(config.statusSets.sets[0].conditions[0].source, statusSlot));
    assert(activeSlots.resolve(kSlotProjectionEvolutionPetStatSource, evolutionSlot));
    assert(statusSlot == 0);
    assert(evolutionSlot == 0);
    assert(strstr(kSlotProjectionEvolutionFixture, "custom0=50..*") != nullptr);
    assert(strstr(kSlotProjectionEvolutionFixture, "custom4") == nullptr);

    PetStatSnapshot stats = {};
    stats.customStats[0] = 50;
    FixtureStatContext statContext = {&activeSlots, &stats};
    StatusSetResolution statusResolution = {};
    assert(resolveStatusSet(
        config.statusSets.sets[0],
        fixtureStatusValue,
        &statContext,
        statusResolution));
    assert(statusResolution.requiredFrames == 2);

    char matchingEvolutionConditions[32] = {};
    strcpy(matchingEvolutionConditions, kSlotProjectionEvolutionConditions);
    assert(evaluateEvolutionConditions(
        matchingEvolutionConditions,
        stats,
        activeSlots));
    stats.customStats[0] = 49;
    char rejectedEvolutionConditions[32] = {};
    strcpy(rejectedEvolutionConditions, kSlotProjectionEvolutionConditions);
    assert(!evaluateEvolutionConditions(
        rejectedEvolutionConditions,
        stats,
        activeSlots));

    assert(decidePetStateSchema(
               kSlotProjectionSchemaFingerprint,
               config.schemaFingerprint) == PetStateSchemaDecision::Restore);
    assert(decidePetStateSchema(
               kSlotProjectionSchemaFingerprint + 1,
               config.schemaFingerprint) == PetStateSchemaDecision::Reset);
}
#endif

bool acceptRecord(void *, const SdTextRecord &)
{
    return true;
}

void testSharedReaderRejectsFileBeyondCallerCapacity()
{
    constexpr const char *contract =
        "runtime_contract|1\n"
        "x\n"
        "crc32|F41DE02E\n";
    assert(!parseSdTextRecords(contract, strlen(contract) - 1, "runtime_contract", "1",
                               acceptRecord, nullptr));
}

void testMalformedContractsFailClosed()
{
    assertRejectedAndCleared("");
    assertRejectedAndCleared(
        "runtime_contract|1\n"
        "pet_behavior|3|00000000\n"
        "crc32|3DEEE7DE\n");
    assertRejectedAndCleared(
        "runtime_contract|1\n"
        "pet_behavior|3|00000000\n"
        "idle|anim0\n"
        "crc32|00000000\n");
    assertRejectedAndCleared(
        "runtime_contract|1\n"
        "pet_behavior|3|00000000\n"
        "idle|anim0\n"
        "button|1|empty|\n"
        "button|2|empty|\n"
        "button|3|empty|\n"
        "button|4|empty|\n"
        "button|5|empty|\n"
        "button|6|empty|\n"
        "button|7|empty|\n"
        "button|8|empty|\n"
        "crc32|00000000\n");
    assertRejectedAndCleared(
        "runtime_contract|1\n"
        "pet_behavior|3|00000000\n"
        "stat|custom8|0|0|1|0\n");
    assertRejectedAndCleared(
        "runtime_contract|1\n"
        "record|1|2|3|4|5|6|7|8\n");
    assertRejectedAndCleared(
        "runtime_contract|1\n"
        "record|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
}
} // namespace

int main()
{
    testMinimalContractParsesCountsAndButtons();
    testSlotsBeyondConfiguredPetStatCapacityAreRejected();
#if ENABLE_GUESS_GAME
    testWebExportedSlotProjectionTargetsOneRuntimeSlot();
#endif
    testSharedReaderRejectsFileBeyondCallerCapacity();
    testMalformedContractsFailClosed();
    return 0;
}
