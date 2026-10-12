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
    Renderer renderer(nullptr, nullptr);
    AnimationController animations(renderer);
    animations.configureRuntimeContract(config);
    animations.setup(config.idleAnimation);
    PetBehaviorRuntime runtime(config, pet, animations, renderer);
    runtime.initializeStats();
    animationLookups = 0;
    missingLookups = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
    assert(pet.customStat(0) == 15 && pet.actionCount(0) == 1);
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
    Renderer renderer(nullptr, nullptr);
    AnimationController animations(renderer);
    animations.setup(config.idleAnimation);
    PetBehaviorRuntime runtime(config, pet, animations, renderer);
    runtime.initializeStats();
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 15 && pet.actionCount(0) == 1);
    assert(!animations.isBusy());
    assert(runtime.advancePetDay() && pet.customStat(0) == 15);
}
} // namespace

Renderer::Renderer(Adafruit_ST7735 *, SdFat *) {}
Renderer::~Renderer() = default;
void Renderer::initAnimations() {}
bool Renderer::setAnimation(const AssetData::AnimationRef &reference, uint8_t, bool)
{
    renderedAnimation = reference.animationId;
    return true;
}
bool Renderer::advanceAnimationFrame() { return true; }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &, uint8_t, uint16_t,
                                  int, int, int, uint16_t, uint16_t) { return true; }
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::animationFrameFailed() const { return false; }
uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &, uint8_t) { return 1; }
uint16_t Renderer::versionCountFor(const AssetData::AnimationRef &reference)
{
    ++animationLookups;
    if (reference.animationId == 2) { ++missingLookups; return 0; }
    return 1;
}
unsigned long Renderer::frameIntervalFor(const AssetData::AnimationRef &, uint8_t,
                                        unsigned long) { return 1; }
SdFat *Renderer::sdCard() const { return nullptr; }

int main()
{
    testStandardEffectsOnly();
    testMatchedEmptyBranchSuppressesFallback();
    testUnmatchedWithoutFallback();
    testRandomEffectsOnly();
    testEmptyPrimaryWithFollowing();
    testReferencedMissingAnimationRetainsCommit();
    return 0;
}
