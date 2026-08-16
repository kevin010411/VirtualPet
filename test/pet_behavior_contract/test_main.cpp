#include <assert.h>
#include <string.h>

#include "pet_behavior/domain/PetBehaviorContract.h"
#include "shared/sd/SdTextRecordReader.h"

namespace
{
constexpr const char *kMinimalContract =
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
    "crc32|AF5C7E68\n";

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

void testRecordsDecodeBySlotWithoutOrderingDependencies()
{
    constexpr const char *contract =
        "runtime_contract|1\n"
        "action_effect|effect0|action7|change|custom7|5\n"
        "button|1|user_action|action7\n"
        "button|2|empty|\n"
        "button|3|empty|\n"
        "button|4|empty|\n"
        "button|5|empty|\n"
        "button|6|empty|\n"
        "button|7|empty|\n"
        "button|8|empty|\n"
        "stat|custom7|5|0|10|-1\n"
        "action|action7|anim31|2\n"
        "idle|anim0\n"
        "pet_behavior|3|12345678\n"
        "crc32|7C3B0036\n";
    PetBehaviorConfig config = {};
    assert(parsePetBehaviorContract(contract, config));
    assert(config.schemaFingerprint == 0x12345678UL);
    assert(config.stats[7].active);
    assert(config.actions[7].active);
    assert(config.actionEffects[0].statSlot == 7);
    assert(config.buttons[0].actionSlot == 7);
}

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
    testRecordsDecodeBySlotWithoutOrderingDependencies();
    testSharedReaderRejectsFileBeyondCallerCapacity();
    testMalformedContractsFailClosed();
    return 0;
}
