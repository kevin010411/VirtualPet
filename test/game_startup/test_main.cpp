#include <assert.h>
#include <string.h>

#include <Adafruit_ST7735.h>
#include "presentation/application/Game.h"
#include "pet/domain/Pet.h"
#include "pet/adapters/PetStorage.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"
#include "pet_behavior/application/PetBehaviorRuntime.h"
#include "animation/application/AnimationController.h"
#include "commands/application/CommandExecutor.h"

unsigned long hostMillis = 0;

namespace
{
struct Scenario
{
    bool initialLoadSucceeds = true;
    bool initialAppearanceResolved = false;
    bool reloadSucceeds = true;
    bool previewSucceeds = true;
    bool saveSucceeds = true;
    bool bundleSucceeds = true;
    bool unlockSucceeds = true;
    EvolutionLookupResult evolutionResult = EvolutionLookupResult::NoTarget;
    uint8_t evolutionSpecies = 2;
    uint8_t initialSpecies = 1;
    uint8_t initialOutfit = 1;
    int initialLoads = 0;
    int reloads = 0;
    int saves = 0;
    int discards = 0;
    int unlocks = 0;
    int evolutionLookups = 0;
    bool lastUnlockInitialize = false;
    int resourceShows = 0;
    char firstRecordedResource[24] = {};
    char lastShownResource[24] = {};
    bool hasSave = false;
    uint16_t actionVersions = 1;
    uint16_t statusVersions = 1;
    Pet savedPet;
};

Scenario *scenario = nullptr;

void copyName(char *destination, size_t capacity, const char *source)
{
    if (capacity == 0) return;
    size_t index = 0;
    while (index + 1 < capacity && source[index] != '\0')
    {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

class FakeAppearanceLoader : public AppearanceLoader
{
public:
    void configureRuntimeContract(const PetBehaviorConfig &) override {}
    const char *firstAssetDataErrorResource() const override { return "appearance"; }
    EvolutionLookupResult findEvolutionTarget(const PetStatSnapshot &,
                                             AppearanceSelection &selection) override
    {
        ++scenario->evolutionLookups;
        if (scenario->evolutionResult == EvolutionLookupResult::Found)
        {
            selection.speciesSlot = scenario->evolutionSpecies;
            selection.outfitSlot = 1;
        }
        return scenario->evolutionResult;
    }
    bool loadSpecies(uint8_t *, size_t, size_t &count) override { count = 0; return true; }
    bool loadOutfits(uint8_t, uint8_t, uint8_t *, size_t, size_t &count) override { count = 0; return true; }
    bool findOutfitPreview(uint8_t species, uint8_t outfit, bool, OutfitPreview &preview) override
    {
        preview.speciesSlot = species;
        preview.outfitSlot = outfit;
        return scenario->previewSucceeds;
    }
    bool resolveOutfitUnlockMask(uint8_t, const PetStatSnapshot &, uint8_t current,
                                 bool initialize, uint8_t &resolved) override
    {
        ++scenario->unlocks;
        scenario->lastUnlockInitialize = initialize;
        resolved = current;
        return scenario->unlockSucceeds;
    }
    bool resolveConsumableOutfitUnlock(uint8_t, uint8_t, const PetStatSnapshot &,
                                       PetStatSnapshot &) override { return false; }
};

void testFreshStartup()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.prepare_game());
    assert(data.initialLoads == 1 && data.reloads == 0);
    assert(data.unlocks == 1 && data.lastUnlockInitialize);
    assert(data.saves == 1);
    assert(pet.speciesSlot() == 1 && pet.outfitSlot() == 1);
    // Preparation loads state but does not enable input or normal ticking.
    assert(!game.setStageDaysForCheat(1));
    hostMillis = 2000;
    game.loop_game();
    assert(pet.stageDays() == 0 && data.evolutionLookups == 0);
    hostMillis = 0;
    assert(game.finish_setup_game());
    assert(data.resourceShows == 0);
}

void testRestoredAppearance(bool different)
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    data.savedPet.setDefaultState();
    data.savedPet.setSchemaFingerprint(123);
    assert(data.savedPet.setSpeciesSlot(different ? 2 : 1));
    assert(data.savedPet.setOutfitSlot(1));
    data.hasSave = true;
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.prepare_game());
    assert(data.initialLoads == 1 && data.reloads == (different ? 1 : 0));
    assert(data.unlocks == 1 && !data.lastUnlockInitialize);
    assert(data.saves == 0);
    assert(pet.speciesSlot() == (different ? 2 : 1));
    assert(game.finish_setup_game());

    assert(game.resetPet());
    assert(data.reloads == (different ? 2 : 1));
    assert(pet.speciesSlot() == 1 && pet.outfitSlot() == 1);
}

void testInvalidSaveFallsBackToFresh()
{
    Scenario data;
    scenario = &data;
    data.hasSave = true;
    data.previewSucceeds = false;
    data.savedPet.setDefaultState();
    data.savedPet.setSchemaFingerprint(123);
    assert(data.savedPet.setSpeciesSlot(2));
    assert(data.savedPet.setOutfitSlot(1));
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.prepare_game());
    assert(data.discards == 1 && data.reloads == 0);
    assert(data.saves == 1);
    assert(pet.speciesSlot() == 1);
    assert(game.finish_setup_game());
}

void testFingerprintMismatchStartsFresh()
{
    Scenario data;
    scenario = &data;
    data.hasSave = true;
    data.savedPet.setDefaultState();
    data.savedPet.setSchemaFingerprint(456);
    assert(data.savedPet.setSpeciesSlot(2));
    assert(data.savedPet.setOutfitSlot(1));
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.prepare_game());
    assert(data.reloads == 0 && data.saves == 1);
    assert(pet.speciesSlot() == 1);
    assert(!pet.isFirstLaunchComplete());
    assert(game.finish_setup_game());
}

void testFreshSaveFailure()
{
    Scenario data;
    scenario = &data;
    data.saveSucceeds = false;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(!game.prepare_game());
    assert(data.saves == 1);
    assert(strcmp(data.firstRecordedResource, "state_a.bin") == 0);
    assert(!game.finish_setup_game());
    assert(strcmp(data.lastShownResource, "state_a.bin") == 0);
    assert(!game.resetPet());
}

void testInvalidInitialState()
{
    Scenario data;
    scenario = &data;
    data.initialSpecies = 0;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(!game.prepare_game());
    assert(data.saves == 0);
    assert(!game.finish_setup_game());
    assert(strcmp(data.lastShownResource, "pet state") == 0);
    assert(!game.resetPet());
}

void testInitialLoadFailure(bool appearanceResolved)
{
    Scenario data;
    scenario = &data;
    data.initialLoadSucceeds = false;
    data.initialAppearanceResolved = appearanceResolved;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(!game.prepare_game());
    // A failed contract cannot be retried even before finish displays the error.
    data.initialLoadSucceeds = true;
    assert(!game.prepare_game());
    assert(data.initialLoads == 1);
    assert(!game.finish_setup_game());
    assert(strcmp(data.lastShownResource,
                  appearanceResolved ? "behavior" : "runtime.bin") == 0);
    assert(strcmp(data.firstRecordedResource, appearanceResolved ? "behavior" : "") == 0);
    assert(!game.resetPet());
    assert(data.saves == 0);
}

void testFinishWithoutPreparation()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(!game.finish_setup_game());
    assert(strcmp(data.lastShownResource, "startup") == 0);
    assert(!game.prepare_game());
    assert(!game.resetPet());
    assert(data.initialLoads == 0 && data.saves == 0);
}

void testStartupActivationFailure(bool bundleFailure)
{
    Scenario data;
    scenario = &data;
    data.bundleSucceeds = !bundleFailure;
    data.unlockSucceeds = bundleFailure;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(!game.setup_game());
    assert(strcmp(data.lastShownResource, bundleFailure ? "runtime.bin" : "appearance") == 0);
    assert(data.saves == 0);
    assert(!game.prepare_game());
    assert(!game.setStageDaysForCheat(1));
    assert(data.initialLoads == 1);
}

void testResetReloadFailure()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.setup_game());
    data.reloadSucceeds = false;
    assert(!game.resetPet());
    assert(data.reloads == 1 && data.saves == 1);
    assert(!game.setStageDaysForCheat(1));
    assert(!game.saveNow());
    assert(!game.resetPet());
    assert(data.reloads == 1 && data.saves == 1);
}

void testRestoredReloadFailure()
{
    Scenario data;
    scenario = &data;
    data.hasSave = true;
    data.reloadSucceeds = false;
    data.savedPet.setDefaultState();
    data.savedPet.setSchemaFingerprint(123);
    assert(data.savedPet.setSpeciesSlot(2));
    assert(data.savedPet.setOutfitSlot(1));
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(!game.prepare_game());
    assert(data.reloads == 1);
    assert(strcmp(data.firstRecordedResource, "restored appearance") == 0);
    assert(!game.finish_setup_game());
    assert(strcmp(data.lastShownResource, "restored appearance") == 0);
    assert(!game.resetPet());
}

void testEvolutionLookup(EvolutionLookupResult result, uint8_t targetSpecies)
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.evolutionResult = result;
    data.evolutionSpecies = targetSpecies;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.prepare_game());
    assert(game.finish_setup_game());
    assert(game.setStageDaysForCheat(1));
    if (result == EvolutionLookupResult::LoadFailed)
        hostMillis = 2000;
    game.loop_game();

    if (result == EvolutionLookupResult::LoadFailed)
    {
        assert(data.reloads == 0);
        assert(data.evolutionLookups == 1);
        assert(strcmp(data.lastShownResource, "appearance") == 0);
        assert(!game.setStageDaysForCheat(2));
    }
    else if (result == EvolutionLookupResult::Found && targetSpecies != 1)
    {
        assert(data.reloads == 1);
        assert(pet.speciesSlot() == targetSpecies);
        assert(data.resourceShows == 0);
    }
    else
    {
        assert(data.reloads == 0);
        assert(pet.speciesSlot() == 1);
        assert(data.resourceShows == 0);
        assert(game.setStageDaysForCheat(2));
    }
    hostMillis = 0;
}

void testEvolutionLoadFailureDuringPetTick()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.evolutionResult = EvolutionLookupResult::LoadFailed;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);

    assert(game.prepare_game());
    assert(game.finish_setup_game());
    hostMillis = 2000;
    game.loop_game();
    assert(data.evolutionLookups == 1);
    assert(strcmp(data.firstRecordedResource, "appearance") == 0);
    assert(strcmp(data.lastShownResource, "appearance") == 0);
    assert(pet.stageDays() == 0);
    game.loop_game();
    assert(strcmp(data.lastShownResource, "appearance") == 0);
    assert(!game.setStageDaysForCheat(1));
    hostMillis = 0;
}
void testBehaviorTransactionsUseLivePet()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    Renderer renderer(nullptr, &sd);
    AnimationController animations(renderer);
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 2, 0, 10, -1};
    config.stats[1] = {true, 3, 0, 10, 1};
    config.actions[0].active = true;
    config.actions[0].mode = PetBehaviorActionMode::Standard;
    config.actions[0].outcomeCount = 1;
    config.actions[0].suspendDailyChangeDays = 2;
    config.actionOutcomeCount = 1;
    config.actionOutcomes[0].animationPlayback = {{0, 0, 1}, 1};
    config.actionOutcomes[0].effectCount = 2;
    config.actionEffectCount = 2;
    config.actionEffects[0] = {0, PetBehaviorEffectOperation::Set, 8};
    config.actionEffects[1] = {1, PetBehaviorEffectOperation::Change, 2};
    PetBehaviorRuntime runtime(config, pet, animations, renderer);
    runtime.initializeStats();
    assert(pet.customStat(0) == 2 && pet.customStat(1) == 3);

    // Reject a duplicate effect after the first effect has been calculated.
    // The real Pet must receive neither a partial commit nor a suspension.
    config.actionEffects[1].statSlot = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Rejected);
    assert(pet.customStat(0) == 2 && pet.customStat(1) == 3);
    assert(runtime.advancePetDay());
    assert(pet.customStat(0) == 1 && pet.customStat(1) == 4);
    assert(pet.stageDays() == 1);

    config.actionEffects[1].statSlot = 1;
    data.actionVersions = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 8 && pet.customStat(1) == 6);
    assert(!animations.isBusy());
    for (int day = 0; day < 2; ++day)
    {
        assert(runtime.advancePetDay());
        assert(pet.customStat(0) == 8 && pet.customStat(1) == 6);
    }
    assert(pet.stageDays() == 3);
    assert(runtime.advancePetDay());
    assert(pet.customStat(0) == 7 && pet.customStat(1) == 7);
    assert(pet.stageDays() == 4);
    assert(data.saves == 0);
}

void testStatusReadsActionCommittedPet()
{
    Scenario data;
    scenario = &data;
    data.statusVersions = 2;
    SdFat sd;
    Pet pet;
    Renderer renderer(nullptr, &sd);
    AnimationController animations(renderer);
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 2, 0, 10, 0};
    config.actions[0].active = true;
    config.actions[0].mode = PetBehaviorActionMode::Standard;
    config.actions[0].outcomeCount = 1;
    config.actionOutcomeCount = 1;
    config.actionOutcomes[0].animationPlayback = {{0, 0, 1}, 1};
    config.actionOutcomes[0].effectCount = 1;
    config.actionEffectCount = 1;
    config.actionEffects[0] = {0, PetBehaviorEffectOperation::Set, 8};
    config.statusSets.count = 1;
    StatusSetConfig &set = config.statusSets.sets[0];
    set.animation = {0, 0, 2};
    set.versionCount = 2;
    set.conditionCount = 1;
    set.conditions[0] = {StatusConditionKind::RuntimeValue,
                         runtimeValueIdForPetStat(0), 0, 2, 0, 10};
    animations.configureRuntimeContract(config);
    PetBehaviorRuntime runtime(config, pet, animations, renderer);
    runtime.initializeStats();
    CommandExecutor executor(pet, animations, runtime);
    executor.configureRuntimeContract(config);
    CommandHost &host = executor;
    executor.begin(AppCommandId::Status);
    host.commandStatus();
    assert(!executor.complete(true).resourceError);
    animations.preparePlayback(0);
    assert(animations.currentVersionIndex() == 0);

    data.actionVersions = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 8);
    executor.begin(AppCommandId::Status);
    host.commandStatus();
    assert(!executor.complete(true).resourceError);
    animations.preparePlayback(1);
    assert(animations.currentVersionIndex() == 1);
    assert(data.saves == 0);
}
} // namespace

bool loadInitialRuntimeContract(SdFat *, AppearanceSelection &selection,
                                PetBehaviorConfig &config, bool &resolved,
                                char *errorResource, size_t capacity)
{
    ++scenario->initialLoads;
    resolved = scenario->initialLoadSucceeds || scenario->initialAppearanceResolved;
    if (!scenario->initialLoadSucceeds)
    {
        copyName(errorResource, capacity, resolved ? "behavior" : "runtime.bin");
        return false;
    }
    selection.speciesSlot = scenario->initialSpecies;
    selection.outfitSlot = scenario->initialOutfit;
    config = {};
    config.schemaFingerprint = 123;
    config.activeSpeciesSlot = selection.speciesSlot;
    config.activeOutfitSlot = selection.outfitSlot;
    return true;
}

bool loadRuntimeContract(SdFat *, uint8_t species, uint8_t outfit,
                         PetBehaviorConfig &config, char *errorResource, size_t capacity)
{
    ++scenario->reloads;
    if (!scenario->reloadSucceeds)
    {
        copyName(errorResource, capacity, "restored appearance");
        return false;
    }
    config.activeSpeciesSlot = species;
    config.activeOutfitSlot = outfit;
    return true;
}

PetStorage::PetStorage(SdFat *) {}
bool PetStorage::load(Pet &pet, uint32_t fingerprint)
{
    if (!scenario->hasSave) return false;
    if (scenario->savedPet.persistentState().schemaFingerprint != fingerprint) return false;
    pet = scenario->savedPet;
    return true;
}
bool PetStorage::save(const Pet &pet)
{
    ++scenario->saves;
    if (!scenario->saveSucceeds) return false;
    scenario->savedPet = pet;
    scenario->hasSave = true;
    return true;
}
void PetStorage::discard() { ++scenario->discards; scenario->hasSave = false; }
char PetStorage::lastSaveSlot() const { return 'A'; }
uint32_t PetStorage::lastSaveSequence() const { return 1; }

Renderer::Renderer(Adafruit_ST7735 *display, SdFat *sd) : tft(display), SD(sd), state(nullptr) {}
Renderer::~Renderer() = default;
void Renderer::initAnimations() {}
void Renderer::setAssetAppearance(uint8_t, uint8_t) {}
void Renderer::setAnimationArea(uint8_t, uint8_t, uint8_t, uint8_t) {}
bool Renderer::configureAssetBundle(const AssetData::BundleId &) { return scenario->bundleSucceeds; }
bool Renderer::setAnimation(const AssetData::AnimationRef &, uint8_t, bool) { return true; }
bool Renderer::currentLayoutId(uint8_t &layout) const { layout = 0; return true; }
bool Renderer::validateLayoutVersion(const AssetData::AnimationRef &,
                                     const AssetData::AnimationRef &, uint8_t, uint8_t, uint16_t) { return true; }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &, uint8_t,
                                  uint16_t, int, int, int, uint16_t, uint16_t) { return true; }
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::advanceAnimationFrame() { return true; }
bool Renderer::animationFrameFailed() const { return false; }
uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &, uint8_t) { return 1; }
uint16_t Renderer::versionCountFor(const AssetData::AnimationRef &animation)
{
    return animation.animationId == 1 ? scenario->actionVersions
         : animation.animationId == 2 ? scenario->statusVersions : 1;
}
unsigned long Renderer::frameIntervalFor(const AssetData::AnimationRef &, uint8_t,
                                         unsigned long defaultInterval) { return defaultInterval; }
void Renderer::showResourceError()
{
    ++scenario->resourceShows;
    copyName(scenario->lastShownResource, sizeof(scenario->lastShownResource),
             scenario->firstRecordedResource);
}
void Renderer::showResourceError(const char *resource)
{
    ++scenario->resourceShows;
    copyName(scenario->lastShownResource, sizeof(scenario->lastShownResource), resource);
}
void Renderer::recordAssetDataErrorResource(const char *resource)
{
    if (scenario->firstRecordedResource[0] == '\0')
        copyName(scenario->firstRecordedResource, sizeof(scenario->firstRecordedResource), resource);
}
AssetData::BundleError Renderer::firstAssetDataError() const { return AssetData::BundleError::None; }
const char *Renderer::firstAssetDataErrorResource() const { return scenario->firstRecordedResource; }
SdFat *Renderer::sdCard() const { return SD; }

int main()
{
    testFreshStartup();
    testRestoredAppearance(false);
    testRestoredAppearance(true);
    testInvalidSaveFallsBackToFresh();
    testFingerprintMismatchStartsFresh();
    testFreshSaveFailure();
    testInvalidInitialState();
    testInitialLoadFailure(false);
    testInitialLoadFailure(true);
    testFinishWithoutPreparation();
    testStartupActivationFailure(true);
    testStartupActivationFailure(false);
    testResetReloadFailure();
    testRestoredReloadFailure();
    testEvolutionLookup(EvolutionLookupResult::NoTarget, 2);
    testEvolutionLookup(EvolutionLookupResult::Found, 1);
    testEvolutionLookup(EvolutionLookupResult::Found, 2);
    testEvolutionLookup(EvolutionLookupResult::LoadFailed, 2);
    testEvolutionLoadFailureDuringPetTick();
    testBehaviorTransactionsUseLivePet();
    testStatusReadsActionCommittedPet();
}
