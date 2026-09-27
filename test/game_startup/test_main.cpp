#include <assert.h>
#include <string.h>

#include <Adafruit_ST7735.h>
#include "presentation/application/Game.h"
#include "pet/domain/Pet.h"
#include "pet/adapters/PetStorage.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"

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
    assert(!game.finish_setup_game());
    assert(strcmp(data.lastShownResource,
                  appearanceResolved ? "behavior" : "runtime.bin") == 0);
    assert(strcmp(data.firstRecordedResource, appearanceResolved ? "behavior" : "") == 0);
    assert(!game.resetPet());
    assert(data.saves == 0);
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
bool Renderer::configureAssetBundle(const AssetData::BundleId &) { return scenario->bundleSucceeds; }
bool Renderer::setAnimation(const AssetData::AnimationRef &, uint8_t, bool) { return true; }
bool Renderer::currentLayoutId(uint8_t &layout) const { layout = 0; return true; }
bool Renderer::validateLayoutVersion(const AssetData::AnimationRef &,
                                     const AssetData::AnimationRef &, uint8_t) { return true; }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &, uint8_t,
                                  uint16_t, int, int, int) { return true; }
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::advanceAnimationFrame() { return true; }
bool Renderer::animationFrameFailed() const { return false; }
uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &, uint8_t) { return 1; }
uint16_t Renderer::versionCountFor(const AssetData::AnimationRef &) { return 1; }
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
    testRestoredReloadFailure();
    testEvolutionLookup(EvolutionLookupResult::NoTarget, 2);
    testEvolutionLookup(EvolutionLookupResult::Found, 1);
    testEvolutionLookup(EvolutionLookupResult::Found, 2);
    testEvolutionLookup(EvolutionLookupResult::LoadFailed, 2);
    testEvolutionLoadFailureDuringPetTick();
}
