#include <assert.h>
#include <fstream>
#include <iterator>
#include <string.h>
#include <string>

#include "pet/adapters/PetStateSchemaDecision.h"
#include "pet_behavior/domain/PetBehaviorContract.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

namespace
{
uint16_t selectSecondWeight(uint16_t)
{
    return 1;
}

constexpr uint32_t kActionModesV11SchemaFingerprint = 0xE6546C8AUL;

void testWebExportedV11ActionModesParseAndExecute(const char *contract)
{
    PetBehaviorConfig config = {};
    assert(parsePetBehaviorContract(contract, config));
    assert(config.schemaFingerprint == kActionModesV11SchemaFingerprint);
    assert(decidePetStateSchema(
               kActionModesV11SchemaFingerprint,
               config.schemaFingerprint) == PetStateSchemaDecision::Restore);
    assert(config.actionCount == 5);
    assert(config.actions[0].mode == PetBehaviorActionMode::Standard);
    assert(config.actions[1].mode == PetBehaviorActionMode::ConditionalAnimation);
    assert(config.actionConditionCount == 4);
    assert(config.actions[2].mode == PetBehaviorActionMode::RandomOutcome);
    assert(config.randomOutcomes[2][0].weight == 1);
    assert(config.randomOutcomes[2][1].weight == 2);
    assert(config.randomOutcomes[2][2].weight == 3);
    assert(config.actionEffectCount == 4);
    assert(config.randomOutcomeEffectCount == 2);

    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    initializePetBehaviorStats(config, state);
    PetBehaviorActionPlayback playback = {};
    assert(applyPetBehaviorAction(config, 1, state, pauses, playback));
    assert(strcmp(playback.animation, "anim2") == 0);
    assert(playback.playbackCount == 2);
    assert(state.values[1] == 100);

    assert(applyPetBehaviorAction(
        config, 2, state, pauses, playback, selectSecondWeight));
    assert(strcmp(playback.animation, "anim9") == 0);
    assert(playback.playbackCount == 2);
    assert(state.values[0] == 5);
    assert(pauses.remainingDays[0] == 2);
}

void testPriorRuntimeContractFailsClosed()
{
    PetBehaviorConfig config = {};
    config.actionCount = 7;
    assert(!parsePetBehaviorContract(
        "runtime_contract|2\n"
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
        "crc32|57A0192C\n",
        config));
    assert(config.actionCount == 0);
}
} // namespace

int main(int argc, char **argv)
{
    assert(argc == 2);
    std::ifstream fixture(argv[1], std::ios::binary);
    assert(fixture);
    const std::string contract{
        std::istreambuf_iterator<char>(fixture),
        std::istreambuf_iterator<char>()};
    assert(!contract.empty());
    testWebExportedV11ActionModesParseAndExecute(contract.c_str());
    testPriorRuntimeContractFailsClosed();
    return 0;
}
