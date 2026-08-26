#include <assert.h>
#include <string.h>

#include "action_modes_v11_fixture.h"
#include "pet/adapters/PetStateSchemaDecision.h"
#include "pet_behavior/domain/PetBehaviorContract.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

namespace
{
uint16_t selectSecondWeight(uint16_t)
{
    return 1;
}

void testWebExportedV11ActionModesParseAndExecute()
{
    PetBehaviorConfig config = {};
    assert(parsePetBehaviorContract(kActionModesV11Fixture, config));
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
        "crc32|00000000\n",
        config));
    assert(config.actionCount == 0);
}
} // namespace

int main()
{
    testWebExportedV11ActionModesParseAndExecute();
    testPriorRuntimeContractFailsClosed();
    return 0;
}
