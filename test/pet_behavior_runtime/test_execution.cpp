#include "resources/AssetResources.h"
#include <assert.h>
#include "animation/AnimationController.h"
#include "common/FirmwareRandom.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorRuntime.h"

namespace
{
uint16_t renderedAnimation;
unsigned animationLookups;
unsigned missingLookups;
uint8_t renderedVersion;
SdFat *effectCard;

PetBehaviorConfig effectsOnlyConfig()
{
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 10, 0, 100, -1};
    config.statCount = 1;
    config.idleAnimation = {1, 1, 1};
    config.actions[0].active = true;
    config.actions[0].outcomeCount = 1;
    config.actions[0].suspendDailyChangeDays = 2;
    config.actionCount = 1;
    config.actionOutcomes[0].effectCount = 1;
    config.actionOutcomeCount = 1;
    config.actionEffects[0] = {0, PetBehaviorEffectOperation::Change, 5};
    config.actionEffectCount = 1;
    config.trackedActions[0] = 0;
    config.trackedActionCount = 1;
    return config;
}

void assertActionApplied(PetBehaviorConfig &config, bool hasFollowing = false)
{
    Pet pet;
    AssetResources resources(nullptr);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
    animations.configureRuntimeContract(config);
    animations.setup(config.idleAnimation);
    PetBehaviorRuntime runtime(config, pet, animations, resources);
    runtime.initializeStats();
    animationLookups = 0;
    missingLookups = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
    assert(pet.customStat(0) == 15 && pet.actionCount(0) == ENABLE_COLLECTION_CATALOG);
    assert(animations.isBusy() == hasFollowing);
    assert(animationLookups == (hasFollowing ? 1U : 0U));
    assert(missingLookups == 0);
    animations.preparePlayback(0);
    animations.tick(0);
    assert(renderedAnimation == (hasFollowing ? 3 : 1));
    animations.preparePlayback(1);
    animations.tick(1);
    animations.preparePlayback(4000);
    animations.tick(4000);
    assert(!animations.isBusy() && renderedAnimation == 1);
    assert(runtime.advancePetDay());
    assert(runtime.advancePetDay());
    assert(pet.customStat(0) == 15 && pet.stageDays() == 2);
    assert(runtime.advancePetDay());
    assert(pet.customStat(0) == 14);
}

void testStandardEffectsOnly()
{
    auto config = effectsOnlyConfig();
    assertActionApplied(config);
}

void testUnchangedEffectRefreshesThroughRuntimeCommit()
{
    const PetBehaviorEffectOperation operations[] = {
        PetBehaviorEffectOperation::Change, PetBehaviorEffectOperation::Set};
    for (const auto operation : operations)
    {
        auto config = effectsOnlyConfig();
        config.stats[0].initialValue = 100;
        config.actionEffects[0] = {0, operation,
            static_cast<int16_t>(operation == PetBehaviorEffectOperation::Set ? 100 : 5)};
        Pet pet;
        AssetResources resources(nullptr);
        Renderer renderer(nullptr, resources);
        AnimationController animations(renderer, resources);
        PetBehaviorRuntime runtime(config, pet, animations, resources);
        runtime.initializeStats();
        assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
        assert(runtime.advancePetDay() && pet.customStat(0) == 100);
        assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
        assert(pet.actionCount(0) == 2 * ENABLE_COLLECTION_CATALOG);
        assert(runtime.advancePetDay() && pet.customStat(0) == 100);
        assert(runtime.advancePetDay() && pet.customStat(0) == 100);
        assert(runtime.advancePetDay() && pet.customStat(0) == 99);
    }
}

void testMatchedEmptyBranchSuppressesFallback()
{
    auto config = effectsOnlyConfig();
    config.actions[0].mode = PetBehaviorActionMode::ConditionalAnimation;
    config.actions[0].conditionCount = 1;
    config.actions[0].hasFallbackAnimation = true;
    config.actionConditionCount = 1;
    config.actionConditions[0].predicate = {runtimeValueIdForPetStat(0), 10, 10};
    config.actionOutcomes[0].animationPlayback = {{1, 1, 2}, 1};
    assertActionApplied(config);
}

void testUnmatchedWithoutFallback()
{
    auto config = effectsOnlyConfig();
    config.actions[0].mode = PetBehaviorActionMode::ConditionalAnimation;
    config.actions[0].conditionCount = 1;
    config.actionConditionCount = 1;
    config.actionConditions[0].predicate = {runtimeValueIdForPetStat(0), 11, 20};
    config.actionConditions[0].animationPlayback = {{1, 1, 2}, 1};
    assertActionApplied(config);
}

void testRandomEffectsOnly()
{
    auto config = effectsOnlyConfig();
    config.actions[0].mode = PetBehaviorActionMode::RandomOutcome;
    config.actions[0].outcomeCount = 2;
    config.actionOutcomeCount = 2;
    config.actionOutcomes[0].weight = 1;
    config.actionOutcomes[1].weight = 1;
    config.actionOutcomes[1].animationPlayback = {{1, 1, 2}, 1};
    // seed 2 selects weight 0; the animated alternative must remain unvisited.
    FirmwareRandom::seed(2);
    assertActionApplied(config);
}

void testEmptyPrimaryWithFollowing()
{
    auto config = effectsOnlyConfig();
    config.actions[0].followingAnimationCount = 1;
    config.followingAnimations[0] = {{1, 1, 3}, 1};
    assertActionApplied(config, true);
}

void testReferencedMissingAnimationRetainsCommit()
{
    auto config = effectsOnlyConfig();
    config.actionOutcomes[0].animationPlayback = {{1, 1, 2}, 1};
    Pet pet;
    AssetResources resources(nullptr);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
    animations.setup(config.idleAnimation);
    PetBehaviorRuntime runtime(config, pet, animations, resources);
    runtime.initializeStats();
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 15 && pet.actionCount(0) == ENABLE_COLLECTION_CATALOG);
    assert(!animations.isBusy());
    assert(runtime.advancePetDay() && pet.customStat(0) == 15);
}
void testFixedVersionReplaysAndPrelude()
{
    auto config = effectsOnlyConfig();
    config.actionOutcomes[0].animationPlayback = {{1, 1, 4}, 3, 28};
    config.actions[0].prelude = {{1, 1, 3}, 1};
    config.actions[0].followingAnimationCount = 1;
    config.followingAnimations[0] = {{1, 1, 5}, 1};
    Pet pet;
    AssetResources resources(nullptr);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
    animations.setup(config.idleAnimation);
    PetBehaviorRuntime runtime(config, pet, animations, resources);
    runtime.initializeStats();
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
    bool sawPrelude = false, sawResult = false, sawFollowing = false;
    for (unsigned tick = 0; tick < 30; ++tick)
    {
        animations.preparePlayback(tick);
        animations.tick(tick);
        sawPrelude |= renderedAnimation == 3;
        if (renderedAnimation == 4) { sawResult = true; assert(renderedVersion == 27); }
        sawFollowing |= renderedAnimation == 5;
        assert(pet.customStat(0) == 15);
    }
    assert(sawPrelude && sawResult && sawFollowing && !animations.isBusy());
    config.actionOutcomes[0].animationPlayback.versionSelection = 29;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 20);
}
void testActualEffectFileReadAndFailureAreAtomic()
{
    auto config = effectsOnlyConfig();
    config.actionEffectsStreamed = true;
    config.actionEffectCount = config.actionOutcomes[0].effectCount = 2;
    config.actionEffectsOffset = 3;
    config.stats[1] = {true, 25, 0, 100, 0};
    uint8_t bytes[27] = {};
    bytes[3] = 0; bytes[4] = 1; bytes[5] = 40; // Stat0 set40
    bytes[15] = 1; bytes[16] = 1; bytes[17] = 50; // Stat1 set50
    SdFat card(bytes, sizeof(bytes));
    effectCard = &card;
    Pet pet;
    AssetResources resources(nullptr);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
    PetBehaviorRuntime runtime(config, pet, animations, resources);
    runtime.initializeStats();
    const auto opens = HostSd::openCount;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
    assert(pet.customStat(0) == 40 && pet.customStat(1) == 50);
    assert(HostSd::openCount == opens + 1);
    config.actionEffectsOffset = 15; // first record succeeds, second physically truncated
    assert(runtime.executeAction(0) == PetBehaviorActionResult::ResourceReadFailed);
    assert(pet.customStat(0) == 40 && pet.customStat(1) == 50);
    effectCard = nullptr;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::ResourceReadFailed);
    assert(pet.customStat(0) == 40 && pet.customStat(1) == 50);
}
} // namespace

AssetResources::AssetResources(SdFat *sd)
    : sd_(effectCard), reader_(sd, readBuffer_, AssetData::kIoScratchBytes) {}
BundleReader::BundleReader(SdFat *sd, uint8_t *scratch, size_t size)
    : sd_(sd), scratch_(scratch), scratchSize_(size) {}

Renderer::Renderer(Adafruit_ST7735 *display, AssetResources &resourceRef)
    : tft(display), resources(resourceRef), state(nullptr) {}
Renderer::~Renderer() = default;
void Renderer::initAnimations() {}
bool Renderer::setAnimation(const AssetData::AnimationRef &reference, uint8_t version, bool)
{
    renderedAnimation = reference.animationId;
    renderedVersion = version;
    return true;
}
bool Renderer::advanceAnimationFrame() { return true; }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &, uint8_t, uint16_t,
                                  int, int, int, uint16_t, uint16_t) { return true; }
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::animationFrameFailed() const { return false; }
uint16_t AssetResources::frameCountFor(const AssetData::AnimationRef &, uint8_t) { return 1; }
uint16_t AssetResources::versionCountFor(const AssetData::AnimationRef &reference)
{
    ++animationLookups;
    if (reference.animationId == 2) { ++missingLookups; return 0; }
    return reference.animationId == 4 ? 28 : 1;
}
unsigned long AssetResources::frameIntervalFor(const AssetData::AnimationRef &, uint8_t,
                                        unsigned long) { return 1; }

int main()
{
    testStandardEffectsOnly();
    testUnchangedEffectRefreshesThroughRuntimeCommit();
    testMatchedEmptyBranchSuppressesFallback();
    testUnmatchedWithoutFallback();
    testRandomEffectsOnly();
    testEmptyPrimaryWithFollowing();
    testReferencedMissingAnimationRetainsCommit();
    testFixedVersionReplaysAndPrelude();
    testActualEffectFileReadAndFailureAreAtomic();
    return 0;
}
