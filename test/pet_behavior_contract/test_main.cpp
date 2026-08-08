#include <assert.h>
#include <string.h>

#include "pet_behavior/domain/PetBehaviorContract.h"

namespace
{
constexpr const char *kMinimalContract =
    "pet_behavior|1|00000000|0|0|0|0|8\n"
    "idle|anim0\n"
    "button|1|empty|\n"
    "button|2|empty|\n"
    "button|3|empty|\n"
    "button|4|empty|\n"
    "button|5|empty|\n"
    "button|6|empty|\n"
    "button|7|empty|\n"
    "button|8|empty|\n"
    "crc32|7B21D0E5\n";

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
    assert(config.healthStatusCount == 0);
    assert(config.actionCount == 0);
    assert(config.actionEffectCount == 0);
    assert(config.buttonCount == 8);
    assert(strcmp(config.idleAnimation, "anim0") == 0);
    for (uint8_t slot = 0; slot < config.buttonCount; ++slot)
        assert(config.buttons[slot].kind == PetBehaviorButtonKind::Empty);
}

void testMalformedContractsFailClosed()
{
    assertRejectedAndCleared("");
    assertRejectedAndCleared(
        "pet_behavior|1|00000000|0|0|0|0|8\n"
        "idle|anim0\n"
        "crc32|00000000\n");
    assertRejectedAndCleared(
        "pet_behavior|1|00000000|0|0|0|0|8\n"
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
}
} // namespace

int main()
{
    testMinimalContractParsesCountsAndButtons();
    testMalformedContractsFailClosed();
    return 0;
}
