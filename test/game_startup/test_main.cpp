#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <Adafruit_ST7735.h>
#include "controller/Game.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "display/Renderer.h"
#include "pet/PetBehaviorTypes.h"
#include "pet/PetBehaviorRuntime.h"
#include "animation/AnimationController.h"
#include "controller/CommandExecutor.h"

unsigned long hostMillis = 0;

namespace
{
struct Scenario
{
    bool firstStartAvailable = true;
    bool rejectFirstStartQueue = false;
    int firstStartFrameQueries = 0;
    bool commandAction = false;
    bool commandOutfit = false;
    int previewQueries = 0;
    int playbackDraws = 0;
    bool layoutSucceeds = true;
    bool initialLoadSucceeds = true;
    bool initialAppearanceResolved = false;
    bool reloadSucceeds = true;
    bool previewSucceeds = true;
    bool saveSucceeds = true;
    bool bundleSucceeds = true;
    bool unlockSucceeds = true;
    bool replaceUnlockMask = false;
    uint8_t resolvedUnlockMask = 0;
    bool consumeSucceeds = false;
    bool invalidConsumedStats = false;
    std::string appearanceEvents;
    PetStatSnapshot unlockInput = {};
    PetStatSnapshot saveInput = {};
    uint8_t rendererSpecies = 0;
    uint8_t rendererOutfit = 0;
    EvolutionLookupResult evolutionResult = EvolutionLookupResult::NoTarget;
    uint8_t evolutionSpecies = 2;
    EvolutionAnimationMode evolutionMode = EvolutionAnimationMode::Disabled;
    uint8_t sourcePlaybackCount = 2;
    uint8_t targetPlaybackCount = 2;
    bool sourceAvailable = true;
    bool targetAvailable = true;
    bool rejectSourceQueue = false;
    bool frameFailed = false;
    uint8_t failedAnimationId = 0;
    bool assetError = false;
    AssetData::AnimationRef displayedAnimation = {};
    int sourceFrameQueries = 0;
    int sourceDraws = 0;
    int targetDraws = 0;
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
void assertFatalBlocksPublicEntries(Game &game, Pet &pet, Scenario &data);

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
            selection.evolutionMode = scenario->evolutionMode;
            selection.sourceEvolutionPlaybackCount = scenario->sourcePlaybackCount;
            selection.targetEvolutionPlaybackCount = scenario->targetPlaybackCount;
            if (selection.evolutionMode != EvolutionAnimationMode::Disabled)
                selection.sourceEvolutionAnimation = {1, 1, 3};
            if (selection.evolutionMode == EvolutionAnimationMode::TwoPhase)
                selection.targetEvolutionAnimation = {scenario->evolutionSpecies, 1, 4};
        }
        return scenario->evolutionResult;
    }
    bool loadOutfits(uint8_t, uint8_t, uint8_t *outfits, size_t, size_t &count) override
    {
        count = scenario->commandOutfit ? 2 : 0;
        if (count) { outfits[0] = 1; outfits[1] = 2; }
        return true;
    }
    bool findOutfitPreview(uint8_t species, uint8_t outfit, bool, OutfitPreview &preview) override
    {
        ++scenario->previewQueries;
        preview.speciesSlot = species;
        preview.outfitSlot = outfit;
        preview.animation = {species, outfit, 5};
        return scenario->previewSucceeds;
    }
    bool resolveOutfitUnlockMask(uint8_t, const PetStatSnapshot &stats, uint8_t current,
                                 bool initialize, uint8_t &resolved) override
    {
        ++scenario->unlocks;
        scenario->appearanceEvents += 'U';
        scenario->unlockInput = stats;
        scenario->lastUnlockInitialize = initialize;
        resolved = scenario->replaceUnlockMask ? scenario->resolvedUnlockMask : current;
        return scenario->unlockSucceeds;
    }
    bool resolveConsumableOutfitUnlock(uint8_t, uint8_t, const PetStatSnapshot &stats,
                                       PetStatSnapshot &consumed) override
    {
        scenario->appearanceEvents += 'C';
        consumed = stats;
        consumed.stage_days -= 2;
        consumed.customStats[0] -= 3;
        if (scenario->invalidConsumedStats) consumed.speciesSlot = 9;
        return scenario->consumeSucceeds;
    }
};

class ChangeHost : public AppearanceChangeHost
{
public:
    explicit ChangeHost(Renderer &rendererRef) : renderer(rendererRef) {}
    bool configureActiveAppearance(uint8_t species, uint8_t outfit) override
    {
        scenario->appearanceEvents += 'L';
        if (!scenario->reloadSucceeds) return false;
        renderer.setAssetAppearance(species, outfit);
        return true;
    }
private:
    Renderer &renderer;
};

class SessionHost : public PetSessionHost
{
public:
    SessionHost(PetBehaviorConfig &configRef, Renderer &rendererRef,
                AnimationController &animationsRef, AppearanceLoader &loaderRef)
        : config(configRef), renderer(rendererRef), animations(animationsRef), loader(loaderRef) {}
    bool activateLoadedAppearance(uint8_t species, uint8_t outfit) override
    {
        if (!renderer.configureAssetBundle(config.assetManifest.bundleId)) return false;
        renderer.setAssetAppearance(species, outfit);
        loader.configureRuntimeContract(config);
        animations.configureRuntimeContract(config);
        return true;
    }
    bool configureActiveAppearance(uint8_t species, uint8_t outfit) override
    {
        return loadRuntimeContract(renderer.sdCard(), species, outfit, config) &&
               activateLoadedAppearance(species, outfit);
    }
private:
    PetBehaviorConfig &config;
    Renderer &renderer;
    AnimationController &animations;
    AppearanceLoader &loader;
};

void testPetSession()
{
    // Exercise the data-flow interface without Game, layout or input setup.
    // Fresh, same/different restore, schema rejection, preview rejection and
    // changed unlock mask all finish with a coherent state, then reset to initial.
    for (int mode = 0; mode < 6; ++mode)
    {
        Scenario data;
        scenario = &data;
        data.commandAction = true;
        data.hasSave = mode != 0;
        data.savedPet.setSchemaFingerprint(mode == 3 ? 456 : 123);
        data.savedPet.setSpeciesSlot(mode == 2 ? 2 : 1);
        data.savedPet.setStageDays(9);
        data.savedPet.setCustomStat(0, 8);
        data.previewSucceeds = mode != 4;
        data.replaceUnlockMask = mode == 5;
        data.resolvedUnlockMask = 3;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader loader;
        AnimationController animations(renderer);
        PetBehaviorConfig config = {};
        PetBehaviorRuntime runtime(config, pet, animations, renderer);
        PetSaveController saves(pet, storage, renderer);
        SessionHost host(config, renderer, animations, loader);
        AppearanceChangeController changes(pet, saves, storage, renderer, loader, host);
        PetSession session(pet, storage, renderer, loader, config, runtime, changes, host);
        assert(session.state() == PetSession::State::NotPrepared &&
               session.reset() == AppearanceChangeResult::PetStateRejected);
        assert(data.initialLoads == 0 && data.saves == 0);
        assert(session.prepare(&sd) && session.state() == PetSession::State::Ready);
        const bool restored = mode == 1 || mode == 2 || mode == 5;
        assert(pet.stageDays() == (restored ? 9u : 0u));
        assert(pet.customStat(0) == (restored ? 8 : 2));
        assert(data.lastUnlockInitialize == !restored);
        assert(data.reloads == (mode == 2 ? 1 : 0));
        assert(data.saves == (!restored || mode == 5 ? 1 : 0));
        assert(data.discards == (mode == 4 ? 1 : 0));
        const int reloads = data.reloads;
        const int saveCount = data.saves;
        assert(session.reset() == AppearanceChangeResult::Applied);
        assert(pet.speciesSlot() == 1 && pet.outfitSlot() == 1);
        assert(pet.stageDays() == 0 && pet.customStat(0) == 2);
        assert(pet.persistentState().schemaFingerprint == 123);
        assert(data.reloads == reloads + 1 && data.saves == saveCount + 1);
        assert(data.lastUnlockInitialize);
        data.reloadSucceeds = false;
        const int messages = data.resourceShows;
        assert(session.reset() == AppearanceChangeResult::ConfigurationFailed);
        assert(data.resourceShows == messages); // Game owns fatal presentation.
    }
    const PetSession::State expected[] = {
        PetSession::State::ContractFailed, PetSession::State::ActiveAppearanceFailed,
        PetSession::State::ActiveAppearanceFailed, PetSession::State::PetStateFailed,
        PetSession::State::RestoredAppearanceFailed, PetSession::State::UnlockFailed,
        PetSession::State::UnlockFailed,
    };
    for (int mode = 0; mode < 7; ++mode)
    {
        Scenario data;
        scenario = &data;
        data.initialLoadSucceeds = mode > 1;
        data.initialAppearanceResolved = mode == 1;
        data.bundleSucceeds = mode != 2;
        data.initialSpecies = mode == 3 ? 0 : 1;
        data.hasSave = mode == 4;
        data.savedPet.setSchemaFingerprint(123);
        data.savedPet.setSpeciesSlot(2);
        data.reloadSucceeds = mode != 4;
        data.unlockSucceeds = mode != 5;
        data.saveSucceeds = mode != 6;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader loader;
        AnimationController animations(renderer);
        PetBehaviorConfig config = {};
        PetBehaviorRuntime runtime(config, pet, animations, renderer);
        PetSaveController saves(pet, storage, renderer);
        SessionHost host(config, renderer, animations, loader);
        AppearanceChangeController changes(pet, saves, storage, renderer, loader, host);
        PetSession session(pet, storage, renderer, loader, config, runtime, changes, host);
        assert(!session.prepare(&sd) && session.state() == expected[mode]);
        assert(session.errorStage() != nullptr &&
               session.reset() == AppearanceChangeResult::PetStateRejected);
        const int loads = data.initialLoads;
        if (mode != 3)
        {
            assert(!session.prepare(&sd));
            assert(data.initialLoads == loads); // Terminal runtime failure.
        }
        else
        {
            data.initialSpecies = 1;
            assert(session.prepare(&sd)); // Pet-state failure remains retryable.
        }
    }
}

class EvolutionTestHost : public EvolutionHost
{
public:
    AppearanceChangeResult result = AppearanceChangeResult::Applied;
    int entries = 0;
    int refreshes = 0;
    AppearanceChangeResult enterSpecies(uint8_t, uint8_t) override
    {
        ++entries;
        return result;
    }
    void refreshBaseAnimation() override { ++refreshes; }
};

void testExportedEvolutionPlaybackCounts()
{
    for (const uint8_t sourceCount : {1, 3, 5})
    for (const uint8_t targetCount : {1, 4, 5})
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.evolutionResult = EvolutionLookupResult::Found;
        data.evolutionMode = EvolutionAnimationMode::TwoPhase;
        data.sourcePlaybackCount = sourceCount;
        data.targetPlaybackCount = targetCount;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, appearance);
        assert(game.setup_game());
        assert(game.setStageDaysForCheat(1));
        const int savesBefore = data.saves;
        for (uint8_t index = 0; index < sourceCount; ++index)
        {
            assert(pet.speciesSlot() == 1 && data.saves == savesBefore);
            game.loop_game();
        }
        assert(data.sourceDraws == sourceCount && pet.speciesSlot() == 2);
        assert(data.saves == savesBefore + 1 && data.targetDraws == 0);
        for (uint8_t index = 0; index < targetCount; ++index)
            game.loop_game();
        assert(data.targetDraws == targetCount && !game.hasTransientAnimation());
        assert(data.reloads == 1 && data.saves == savesBefore + 1);
    }
}

void testEvolutionResults()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    pet.setSpeciesSlot(1);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader loader;
    AnimationController animations(renderer);
    EvolutionTestHost host;
    EvolutionController evolution(pet, animations, loader, renderer, host);
    assert(evolution.check() == EvolutionResult::NoChange);
    assert(evolution.update(PlaybackResult::Accepted) == EvolutionResult::NoChange);
    data.evolutionResult = EvolutionLookupResult::LoadFailed;
    assert(evolution.check() == EvolutionResult::FatalFailure);
    assert(host.entries == 0 && data.resourceShows == 0);
    data.evolutionResult = EvolutionLookupResult::Found;
    data.evolutionSpecies = 1;
    assert(evolution.check() == EvolutionResult::NoChange);
    data.evolutionSpecies = 2;
    for (const auto result : {AppearanceChangeResult::PetStateRejected,
                             AppearanceChangeResult::UnlockFailed,
                             AppearanceChangeResult::SaveFailed,
                             AppearanceChangeResult::ConfigurationFailed,
                             AppearanceChangeResult::Applied})
    {
        host.result = result;
        const EvolutionResult expected = result == AppearanceChangeResult::Applied
            ? EvolutionResult::Completed : result == AppearanceChangeResult::ConfigurationFailed
            ? EvolutionResult::FatalFailure : EvolutionResult::Failed;
        assert(evolution.check() == expected);
        assert(!evolution.isActive() && data.resourceShows == 0);
    }
    assert(host.refreshes == 1);
    data.evolutionMode = EvolutionAnimationMode::Single;
    data.sourceAvailable = false;
    assert(evolution.check() == EvolutionResult::Failed && !evolution.isActive());
    data.sourceAvailable = true;
    assert(evolution.check() == EvolutionResult::InProgress && evolution.isActive());
    const int entries = host.entries;
    assert(evolution.check() == EvolutionResult::InProgress);
    assert(evolution.update(PlaybackResult::Accepted) == EvolutionResult::InProgress);
    assert(host.entries == entries);
    assert(evolution.update(PlaybackResult::PlaybackFailed) == EvolutionResult::Failed);
    assert(!evolution.isActive() && data.resourceShows == 0);
    // Contract failure after the source segment is returned to Game unchanged.
    assert(evolution.check() == EvolutionResult::InProgress);
    animations.cancelAll();
    host.result = AppearanceChangeResult::ConfigurationFailed;
    assert(evolution.update(PlaybackResult::Accepted) == EvolutionResult::FatalFailure);
    assert(!evolution.isActive() && data.resourceShows == 0);
}

void testAppearanceChanges()
{
    // Species entry: contract failure, invalid outfit after species mutation,
    // unlock failure, save failure, success, and preactivated startup contract.
    for (int failure = 0; failure < 6; ++failure)
    {
        Scenario data;
        scenario = &data;
        SdFat sd;
        Pet pet;
        pet.setSpeciesSlot(1);
        pet.setOutfitSlot(1);
        pet.setStageDays(10);
        pet.initializeOutfitUnlockMask(1);
        pet.setCustomStat(0, 9);
        data.replaceUnlockMask = true;
        data.resolvedUnlockMask = 3;
        data.reloadSucceeds = failure != 0;
        data.unlockSucceeds = failure != 2;
        data.saveSucceeds = failure != 3;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader loader;
        PetSaveController saves(pet, storage, renderer);
        ChangeHost host(renderer);
        AppearanceChangeController changes(pet, saves, storage, renderer, loader, host);
        const auto result = changes.changeSpecies(2, failure == 1 ? 0 : 1,
            failure == 5 ? AppearanceChangeController::ContractState::AlreadyActivated
                         : AppearanceChangeController::ContractState::Reload);
        const AppearanceChangeResult expected[] = {
            AppearanceChangeResult::ConfigurationFailed, AppearanceChangeResult::PetStateRejected,
            AppearanceChangeResult::UnlockFailed, AppearanceChangeResult::SaveFailed,
            AppearanceChangeResult::Applied, AppearanceChangeResult::Applied};
        const char *events[] = {"L", "LR", "LRU", "LRUS", "LRUS", "US"};
        assert(result == expected[failure]);
        assert(data.appearanceEvents == events[failure]);
        assert(pet.speciesSlot() == (failure == 0 ? 1 : 2));
        assert(pet.outfitSlot() == 1 && pet.customStat(0) == 9);
        assert(pet.stageDays() == (failure == 0 ? 10 : 0));
        assert(pet.outfitUnlockMask() == (failure == 0 ? 1 : failure < 3 ? 0 : 3));
        assert(data.saves == (failure >= 3 ? 1 : 0));
        if (failure >= 2)
            assert(data.unlockInput.speciesSlot == 2 && data.unlockInput.stage_days == 0);
        if (failure >= 3)
            assert(data.saveInput.speciesSlot == 2 && data.saveInput.stage_days == 0);
        if (failure == 1) assert(strcmp(data.firstRecordedResource, "pet appearance") == 0);
        if (failure == 2) assert(strcmp(data.firstRecordedResource, "appearance") == 0);
        if (failure == 3) assert(strcmp(data.firstRecordedResource, "state_a.bin") == 0);
    }

    // Outfit application keeps contract activation even if the transaction
    // fails, and keeps deductions, unlock and outfit when saving fails.
    for (int failure = 0; failure < 7; ++failure)
    {
        Scenario data;
        scenario = &data;
        SdFat sd;
        Pet pet;
        pet.setSpeciesSlot(1);
        pet.setOutfitSlot(1);
        pet.setStageDays(10);
        pet.setCustomStat(0, 9);
        pet.initializeOutfitUnlockMask(1);
        data.reloadSucceeds = failure != 0;
        data.consumeSucceeds = failure != 1;
        data.invalidConsumedStats = failure == 2;
        data.saveSucceeds = failure != 3 && failure != 5;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader loader;
        PetSaveController saves(pet, storage, renderer);
        ChangeHost host(renderer);
        AppearanceChangeController changes(pet, saves, storage, renderer, loader, host);
        const auto result = changes.applyOutfit(2, failure < 5);
        const AppearanceChangeResult expected[] = {
            AppearanceChangeResult::ConfigurationFailed, AppearanceChangeResult::UnlockFailed,
            AppearanceChangeResult::PetStateRejected, AppearanceChangeResult::SaveFailed,
            AppearanceChangeResult::Applied, AppearanceChangeResult::SaveFailed,
            AppearanceChangeResult::Applied};
        const char *events[] = {"L", "LRC", "LRC", "LRCRS", "LRCRS", "LRRS", "LRRS"};
        assert(result == expected[failure]);
        assert(data.appearanceEvents == events[failure]);
        assert(pet.outfitSlot() == (failure < 3 ? 1 : 2));
        assert(pet.stageDays() == (failure == 3 || failure == 4 ? 8 : 10));
        assert(pet.customStat(0) == (failure == 3 || failure == 4 ? 6 : 9));
        assert(pet.outfitUnlockMask() == (failure == 3 || failure == 4 ? 3 : 1));
        assert(data.rendererOutfit == (failure == 0 ? 0 : 2));
        assert(data.firstRecordedResource[0] == '\0');
    }
}

void testUnlockRefreshAndSaveCadence()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    pet.setSpeciesSlot(1);
    pet.setOutfitSlot(1);
    pet.initializeOutfitUnlockMask(1);
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader loader;
    PetSaveController saves(pet, storage, renderer);
    ChangeHost host(renderer);
    AppearanceChangeController changes(pet, saves, storage, renderer, loader, host);
    assert(changes.refreshUnlockState(false) && data.saves == 0);
    saves.maybeSave();
    assert(data.saves == 0);
    data.replaceUnlockMask = true;
    data.resolvedUnlockMask = 3;
    data.saveSucceeds = false;
    assert(!changes.refreshUnlockState(false));
    assert(pet.outfitUnlockMask() == 3 && data.saves == 1);
    // Failed immediate save does not clear the pending periodic tick.
    saves.maybeSave();
    assert(data.saves == 2);
    saves.maybeSave();
    assert(data.saves == 3);
    data.saveSucceeds = true;
    assert(saves.saveNow() && data.saves == 4);
    saves.maybeSave();
    assert(data.saves == 4);
    saves.maybeSave();
    assert(data.saves == 5);
    data.unlockSucceeds = false;
    data.resolvedUnlockMask = 7;
    assert(!changes.refreshUnlockState(true));
    assert(pet.outfitUnlockMask() == 3 && data.saves == 5);
}

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
    assert(data.resourceShows == 0); // TFT is initialized between prepare and finish.
    // A failed contract cannot be retried even before finish displays the error.
    data.initialLoadSucceeds = true;
    assert(!game.prepare_game());
    assert(data.initialLoads == 1);
    assert(data.resourceShows == 0);
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

enum class EvolutionFailure
{
    None, MissingSource, RejectedSource, MissingTarget, SourceFrame, TargetFrame,
    Contract, Save, Asset, TargetAsset,
};

void testAnimatedEvolution(EvolutionAnimationMode mode, EvolutionFailure failure,
                           bool redraw = false)
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.evolutionResult = EvolutionLookupResult::Found;
    data.evolutionMode = mode;
    data.sourceAvailable = failure != EvolutionFailure::MissingSource;
    data.targetAvailable = failure != EvolutionFailure::MissingTarget;
    data.rejectSourceQueue = failure == EvolutionFailure::RejectedSource;
    data.assetError = failure == EvolutionFailure::Asset || failure == EvolutionFailure::TargetAsset;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);
    assert(game.setup_game());
    assert(game.setStageDaysForCheat(1));
    const int savesBefore = data.saves;
    data.reloadSucceeds = failure != EvolutionFailure::Contract;
    data.saveSucceeds = failure != EvolutionFailure::Save;
    if (failure == EvolutionFailure::SourceFrame || failure == EvolutionFailure::Asset)
        data.failedAnimationId = 3;
    if (failure == EvolutionFailure::TargetFrame || failure == EvolutionFailure::TargetAsset)
        data.failedAnimationId = 4;

    // Starts and renders the first source repetition, without advancing a day.
    game.loop_game();
    if (failure == EvolutionFailure::MissingSource || failure == EvolutionFailure::RejectedSource ||
        failure == EvolutionFailure::SourceFrame)
    {
        assert(pet.speciesSlot() == 1 && data.reloads == 0 && data.saves == savesBefore);
        assert(data.resourceShows > 0);
        assert(!game.hasTransientAnimation());
        assert(game.setStageDaysForCheat(1));
        return;
    }
    if (failure == EvolutionFailure::Asset)
    {
        assert(pet.speciesSlot() == 1 && data.reloads == 0 && data.saves == savesBefore);
        assert(!game.hasTransientAnimation());
        assert(!game.setStageDaysForCheat(1));
        assert(strcmp(data.lastShownResource, "source.data") == 0);
        return;
    }

    assert(data.sourceDraws == 1 && pet.speciesSlot() == 1);
    assert(data.saves == savesBefore && data.reloads == 0 && pet.stageDays() == 1);
    assert(!game.setStageDaysForCheat(2));
    game.OnLeftKey();
    game.OnRightKey();
    game.OnConfirmKey();
    assert(data.saves == savesBefore && data.evolutionLookups == 1);
    game.loop_game();
    assert(data.sourceDraws == 2 && data.reloads == 1);
    if (failure == EvolutionFailure::Contract)
    {
        assert(pet.speciesSlot() == 1 && data.saves == savesBefore);
        assert(!game.setStageDaysForCheat(1));
        assert(strcmp(data.lastShownResource, "restored appearance") == 0);
        return;
    }
    assert(pet.speciesSlot() == 2 && pet.stageDays() == 0);
    assert(data.saves == savesBefore + 1);
    if (failure == EvolutionFailure::Save)
    {
        assert(data.savedPet.speciesSlot() == 1);
        assert(data.targetDraws == 0 && data.resourceShows > 0);
        assert(!game.hasTransientAnimation());
        return;
    }
    assert(data.savedPet.speciesSlot() == 2 && data.targetDraws == 0);

    if (mode == EvolutionAnimationMode::TwoPhase && failure != EvolutionFailure::MissingTarget)
    {
        assert(!game.setStageDaysForCheat(2));
        if (redraw)
            game.redrawAllNow();
        else
            game.loop_game();
        assert(data.targetDraws == 1);
        if (failure == EvolutionFailure::TargetAsset)
        {
            assert(pet.speciesSlot() == 2 && data.savedPet.speciesSlot() == 2);
            assert(data.saves == savesBefore + 1 && data.reloads == 1);
            assert(!game.hasTransientAnimation() && !game.setStageDaysForCheat(1));
            assert(strcmp(data.lastShownResource, "target.data") == 0);
            return;
        }
        if (failure != EvolutionFailure::TargetFrame)
        {
            assert(game.hasTransientAnimation());
            game.loop_game();
            assert(data.targetDraws == 2);
        }
    }
    assert(!game.hasTransientAnimation());
    assert(data.saves == savesBefore + 1 && data.reloads == 1);
    assert(data.resourceShows == (failure == EvolutionFailure::MissingTarget ? 1 : 0));
    // Completion/failure must render the new base, with no repeated commit.
    game.redrawAllNow();
    assert(data.displayedAnimation.animationId == 5 && data.displayedAnimation.speciesSlot == 2);
    assert(game.setStageDaysForCheat(1));
}

void testEvolutionInterrupted(bool battery, bool afterCommit)
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.evolutionResult = EvolutionLookupResult::Found;
    data.evolutionMode = EvolutionAnimationMode::TwoPhase;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(nullptr, &sd);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, appearance);
    assert(game.setup_game() && game.setStageDaysForCheat(1));
    game.loop_game();
    if (afterCommit) game.loop_game();
    const int savesBefore = data.saves;
    if (battery)
    {
        game.startBatteryAnimation();
        game.endBatteryAnimation();
        game.redrawAllNow();
        assert(pet.speciesSlot() == (afterCommit ? 2 : 1));
        assert(data.saves == savesBefore);
    }
    else
    {
        assert(game.resetPet());
        game.redrawAllNow();
        // FirstStart completes on this redraw and persists its separate record.
        assert(pet.speciesSlot() == 1 &&
               data.saves == savesBefore + 1 + ENABLE_FIRST_START_ANIMATION);
        assert(pet.isFirstStartCompleted() == static_cast<bool>(ENABLE_FIRST_START_ANIMATION));
    }
    assert(data.targetDraws == 0);
    assert(game.setStageDaysForCheat(1));
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

void testGameRoutesUserActionResult()
{
    for (bool missing : {false, true})
    {
        Scenario data;
        scenario = &data;
        data.commandAction = true;
        data.actionVersions = missing ? 0 : 1;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader loader;
        Game game(pet, storage, renderer, loader);
        assert(game.prepare_game() && game.finish_setup_game());
        const int unlocks = data.unlocks;
        const int errors = data.resourceShows;
        const int saves = data.saves;
        assert(pet.customStat(0) == 2);
        game.OnConfirmKey();
        assert(pet.customStat(0) == 8);
        assert(data.unlocks == unlocks + 1);
        assert(data.unlockInput.customStats[0] == 8);
        assert(data.resourceShows == errors + (missing ? 1 : 0));
        assert(data.saves == saves);
        assert(game.hasTransientAnimation() == !missing);
    }
}

void testUnifiedCommandExecution()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    Renderer renderer(nullptr, &sd);
    AnimationController animations(renderer);
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 2, 0, 10, -1};
    config.actions[0].active = true;
    config.actions[0].mode = PetBehaviorActionMode::Standard;
    config.actions[0].outcomeCount = 1;
    config.actions[0].suspendDailyChangeDays = 1;
    config.actionOutcomeCount = 1;
    config.actionOutcomes[0].animationPlayback = {{0, 0, 1}, 1};
    config.actionOutcomes[0].effectCount = 1;
    config.actionEffectCount = 1;
    config.actionEffects[0] = {0, PetBehaviorEffectOperation::Set, 8};
    // Button position and referenced Action slot are deliberately different.
    config.buttons[3] = {true, PetBehaviorButtonKind::UserAction, 0, RuntimeSystemCommandId::Status};
    config.buttons[4] = {true, PetBehaviorButtonKind::UserAction, 1, RuntimeSystemCommandId::Status};
    config.buttons[5] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::Status};
    config.screenBlockCount = 3;
    for (uint8_t index = 0; index < 3; ++index)
    {
        config.screenBlocks[index].kind = ScreenBlockKind::Button;
        config.screenBlocks[index].source = index + 4;
    }
    animations.configureRuntimeContract(config);
    PetBehaviorRuntime runtime(config, pet, animations, renderer);
    runtime.initializeStats();
    CommandExecutor executor(pet, animations, runtime);
    executor.configureRuntimeContract(config);
    CommandController commands;
    commands.configure(config);
    // Selection starts on an invisible slot; it cannot apply effects.
    assert(!executor.execute(commands).executed);
    assert(pet.customStat(0) == 2);
    commands.resetSelection();
    assert(commands.selectedSlot() == 3);
    const CommandResult applied = executor.execute(commands);
    assert(applied.executed && applied.commandId == AppCommandId::UserAction);
    assert(applied.actionResult == PetBehaviorActionResult::Applied && !applied.resourceError);
    assert(!applied.requestedOutfit && !applied.requestedMinigame);
    assert(pet.customStat(0) == 8 && animations.isBusy());
    assert(runtime.advancePetDay() && pet.customStat(0) == 8);
    assert(runtime.advancePetDay() && pet.customStat(0) == 7);
    animations.cancelAll();
    data.actionVersions = 0;
    const CommandResult missing = executor.execute(commands);
    assert(missing.executed && missing.resourceError);
    assert(missing.actionResult == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 8 && !animations.isBusy());
    assert(runtime.advancePetDay() && pet.customStat(0) == 8);
    commands.next();
    const CommandResult rejected = executor.execute(commands);
    assert(!rejected.executed && !rejected.resourceError);
    assert(rejected.actionResult == PetBehaviorActionResult::Rejected);
    assert(pet.customStat(0) == 8);
    // A subsequent System Command starts with a fresh result.
    commands.next();
    const CommandResult status = executor.execute(commands);
    assert(status.executed && status.commandId == AppCommandId::Status && status.resourceError);
    assert(status.actionResult == PetBehaviorActionResult::Rejected);
    assert(!status.requestedOutfit && !status.requestedMinigame);
    config.buttons[5].active = false;
    commands.configure(config);
    assert(!executor.execute(commands).executed);
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
    config.buttons[0] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::Status};
    config.screenBlockCount = 1;
    config.screenBlocks[0].kind = ScreenBlockKind::Button;
    config.screenBlocks[0].source = 1;
    CommandController commands;
    commands.configure(config);
    assert(!executor.execute(commands).resourceError);
    animations.preparePlayback(0);
    assert(animations.currentVersionIndex() == 0);

    data.actionVersions = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 8);
    assert(!executor.execute(commands).resourceError);
    animations.preparePlayback(1);
    assert(animations.currentVersionIndex() == 1);
    assert(data.saves == 0);
}
#if ENABLE_FIRST_START_ANIMATION
void testFirstStartRecord()
{
    // Completion, failed save, failed playback and missing animation remain
    // distinct from the retired FirstLaunch selection gate.
    for (int mode = 0; mode < 5; ++mode)
    {
        Scenario data;
        scenario = &data;
        data.commandAction = true;
        hostMillis = 0;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, appearance);
        assert(game.prepare_game() && game.finish_setup_game());
        assert(!pet.isFirstStartCompleted());
        assert(!pet.isFirstLaunchComplete());
        const int initialSaves = data.saves;
        data.saveSucceeds = mode != 1;
        data.failedAnimationId = mode == 2 ? 6 : 0;
        data.firstStartAvailable = mode != 3;
        data.rejectFirstStartQueue = mode == 4;
        assert(game.startStartupAnimation() == (mode < 3));
        if (mode < 3)
        {
            game.loop_game();
            assert(pet.isFirstStartCompleted() == (mode == 0));
            assert(data.saves == initialSaves + (mode < 2 ? 1 : 0));
            assert(data.savedPet.isFirstStartCompleted() == (mode == 0));
        }
        else
        {
            assert(data.saves == initialSaves);
            assert(!pet.isFirstStartCompleted());
        }
        if (mode >= 2)
            assertFatalBlocksPublicEntries(game, pet, data);
        assert(!pet.isFirstLaunchComplete());
        if (mode == 0)
        {
            // After startup, input reaches commands without a selection gate.
            hostMillis = 2000;
            game.loop_game();
            game.OnConfirmKey();
            assert(pet.customStat(0) == 8);
            // Restore keeps the completion record, while reset starts a new life.
            Game restored(pet, storage, renderer, appearance);
            assert(restored.prepare_game() && restored.finish_setup_game());
            assert(pet.isFirstStartCompleted());
            assert(!restored.startStartupAnimation());
            const int completedSaves = data.saves;
            assert(restored.resetPet());
            assert(!pet.isFirstStartCompleted());
            restored.loop_game();
            assert(pet.isFirstStartCompleted());
            assert(data.saves == completedSaves + 2);
        }
    }
}

void testResetStartupFatal()
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
    data.firstStartAvailable = false;
    assert(!game.resetPet());
    assert(!pet.isFirstStartCompleted());
    assertFatalBlocksPublicEntries(game, pet, data);
}
#endif
void assertFatalBlocksPublicEntries(Game &game, Pet &pet, Scenario &data)
{
    assert(!game.hasTransientAnimation());
    const int draws = data.playbackDraws;
    const int previews = data.previewQueries;
    const int saves = data.saves;
    const int reloads = data.reloads;
    const int lookups = data.evolutionLookups;
    const uint32_t days = pet.stageDays();
    const uint8_t outfit = pet.outfitSlot();
    game.OnLeftKey();
    game.OnRightKey();
    game.OnConfirmKey();
    game.requestFullRedraw();
    game.redrawAllNow();
    game.startBatteryAnimation();
    game.updateBatteryAnimation(9000);
    game.endBatteryAnimation();
    hostMillis = 9000;
    game.loop_game();
    assert(!game.startStartupAnimation());
    assert(!game.saveNow());
    assert(!game.resetPet());
    assert(!game.setStageDaysForCheat(days + 1));
    assert(!game.prepare_game());
    assert(!game.finish_setup_game()); // A Ready Session must not reopen a fatal Game.
    assert(!game.hasTransientAnimation());
    assert(data.playbackDraws == draws && data.previewQueries == previews);
    assert(data.saves == saves && data.reloads == reloads && data.evolutionLookups == lookups);
    assert(pet.stageDays() == days && pet.outfitSlot() == outfit);
}

void testFatalPublicEntries()
{
    // Both loop's pre-existing resource error and playback's resource error
    // must stop queued playback. Also cover an active appearance menu.
    for (int mode = 0; mode < 3; ++mode)
    {
#if !ENABLE_COMMAND_OUTFIT
        if (mode == 2) continue;
#endif
        Scenario data;
        scenario = &data;
        hostMillis = 0;
        data.commandAction = mode != 2;
        data.commandOutfit = mode == 2;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        Renderer renderer(nullptr, &sd);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, appearance);
        assert(game.setup_game());
        game.OnConfirmKey();
        if (mode == 2)
        {
            const int previews = data.previewQueries;
            game.OnRightKey();
            assert(data.previewQueries == previews + 1); // Menu is actually active.
        }
        else
            assert(game.hasTransientAnimation());
        data.assetError = true;
        if (mode == 1)
        {
            data.failedAnimationId = 1;
            game.redrawAllNow();
        }
        else
        {
            data.frameFailed = true;
            renderer.recordAssetDataErrorResource("shared.data");
            game.loop_game();
        }
        assertFatalBlocksPublicEntries(game, pet, data);
    }
}

void testFinishLayoutFailure()
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
    assert(data.resourceShows == 0);
    data.layoutSucceeds = false;
    assert(!game.finish_setup_game());
    assert(data.resourceShows == 1);
    data.layoutSucceeds = true;
    assertFatalBlocksPublicEntries(game, pet, data);
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
    config.idleAnimation = {selection.speciesSlot, selection.outfitSlot, 5};
    config.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::FirstStart)] =
        {selection.speciesSlot, selection.outfitSlot, 6};
    if (scenario->commandAction)
    {
        config.stats[0] = {true, 2, 0, 10, 0};
        config.actions[0].active = true;
        config.actions[0].mode = PetBehaviorActionMode::Standard;
        config.actions[0].outcomeCount = 1;
        config.actionOutcomeCount = 1;
        config.actionOutcomes[0].animationPlayback = {{1, 1, 1}, 1};
        config.actionOutcomes[0].effectCount = 1;
        config.actionEffectCount = 1;
        config.actionEffects[0] = {0, PetBehaviorEffectOperation::Set, 8};
        config.buttons[0] = {true, PetBehaviorButtonKind::UserAction, 0, RuntimeSystemCommandId::Status};
        config.screenBlockCount = 1;
        config.screenBlocks[0].kind = ScreenBlockKind::Button;
        config.screenBlocks[0].source = 1;
    }
    if (scenario->commandOutfit)
    {
        config.buttons[0] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::ChangeOutfit};
        config.screenBlockCount = 1;
        config.screenBlocks[0].kind = ScreenBlockKind::Button;
        config.screenBlocks[0].source = 1;
    }
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
    config.idleAnimation = {species, outfit, 5};
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
    scenario->appearanceEvents += 'S';
    scenario->saveInput = pet.statSnapshot();
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
void Renderer::setAssetAppearance(uint8_t species, uint8_t outfit)
{
    scenario->appearanceEvents += 'R';
    scenario->rendererSpecies = species;
    scenario->rendererOutfit = outfit;
}
void Renderer::setAnimationArea(uint8_t, uint8_t, uint8_t, uint8_t) {}
bool Renderer::configureAssetBundle(const AssetData::BundleId &) { return scenario->bundleSucceeds; }
bool Renderer::setAnimation(const AssetData::AnimationRef &animation, uint8_t, bool)
{
    scenario->displayedAnimation = animation;
    return true;
}
bool Renderer::currentLayoutId(uint8_t &layout) const { layout = 0; return scenario->layoutSucceeds; }
bool Renderer::validateLayoutVersion(const AssetData::AnimationRef &,
                                     const AssetData::AnimationRef &, uint8_t, uint8_t, uint16_t) { return true; }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &, uint8_t,
                                  uint16_t, int, int, int, uint16_t, uint16_t)
{
    ++scenario->playbackDraws;
    return true;
}
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::advanceAnimationFrame()
{
    ++scenario->playbackDraws;
    const uint8_t id = scenario->displayedAnimation.animationId;
    if (id == 3) ++scenario->sourceDraws;
    if (id == 4) ++scenario->targetDraws;
    scenario->frameFailed = id != 0 && id == scenario->failedAnimationId;
    if (scenario->frameFailed && scenario->evolutionMode != EvolutionAnimationMode::Disabled &&
        scenario->assetError)
        recordAssetDataErrorResource(id == 3 ? "source.data" : "target.data");
    return true;
}
bool Renderer::animationFrameFailed() const { return scenario->frameFailed; }
uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &animation, uint8_t)
{
    if (animation.animationId == 6)
    {
        ++scenario->firstStartFrameQueries;
        if (!scenario->firstStartAvailable ||
            (scenario->rejectFirstStartQueue && scenario->firstStartFrameQueries >= 2)) return 0;
    }
    if (animation.animationId == 3)
    {
        ++scenario->sourceFrameQueries;
        if (!scenario->sourceAvailable ||
            (scenario->rejectSourceQueue && scenario->sourceFrameQueries == 2)) return 0;
    }
    if (animation.animationId == 4 && !scenario->targetAvailable) return 0;
    return 1;
}
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
AssetData::BundleError Renderer::firstAssetDataError() const
{
    return scenario->assetError && scenario->frameFailed
               ? static_cast<AssetData::BundleError>(1) : AssetData::BundleError::None;
}
const char *Renderer::firstAssetDataErrorResource() const { return scenario->firstRecordedResource; }
SdFat *Renderer::sdCard() const { return SD; }

int main()
{
    testFatalPublicEntries();
    testFinishLayoutFailure();
#if ENABLE_FIRST_START_ANIMATION
    testFirstStartRecord();
    testResetStartupFatal();
#endif
    testPetSession();
    testEvolutionResults();
    testExportedEvolutionPlaybackCounts();
    testAppearanceChanges();
    testUnlockRefreshAndSaveCadence();
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
    testAnimatedEvolution(EvolutionAnimationMode::Single, EvolutionFailure::None);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::None);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::None, true);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::MissingSource);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::RejectedSource);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::MissingTarget);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::SourceFrame);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::TargetFrame);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::Contract);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::Save);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::Asset);
    testAnimatedEvolution(EvolutionAnimationMode::TwoPhase, EvolutionFailure::TargetAsset);
    testEvolutionInterrupted(false, false);
    testEvolutionInterrupted(false, true);
    testEvolutionInterrupted(true, false);
    testEvolutionInterrupted(true, true);
    testBehaviorTransactionsUseLivePet();
    testGameRoutesUserActionResult();
    testUnifiedCommandExecution();
    testStatusReadsActionCommittedPet();
}
