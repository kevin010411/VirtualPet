#include "resources/AssetResources.h"
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
#include "common/FirmwareRandom.h"
#if ENABLE_COLLECTION_CATALOG
#include "collection/CollectionCatalog.h"
#endif

unsigned long hostMillis = 0;
#if ENABLE_SEQUENCE_AUDIO
namespace SequenceAudioPlatform
{
bool running = false;
uint32_t startedAt = 0;
bool prepare(SdFat *, const SequenceAudioRef &) { running = false; return true; }
bool start() { running = true; startedAt = hostMillis; return true; }
void service() {}
uint32_t consumedSamples() { return uint32_t(hostMillis - startedAt) * 8; }
bool active() { return running; }
SequenceAudioError error() { return SequenceAudioError::None; }
void stop(bool) { running = false; }
}
#endif

void rtcLoop(Game &game)
{
    game.synchronizeTime(static_cast<uint32_t>(hostMillis / 1000));
    game.loop_game();
}

namespace
{
struct Scenario
{
    bool idleMusic = false;
    bool catchUpEvents = false;
    bool batchCatchUp = false;
    bool catchUpReadSucceeds = true;
    int catchUpQueries = 0;
    uint32_t firstUnlockDay = 0;
    bool deathEnabled = false;
    bool firstStartAvailable = true;
    bool rejectFirstStartQueue = false;
    int firstStartFrameQueries = 0;
    bool commandAction = false;
    bool commandPredict = false;
    bool commandInstruction = false;
    bool instructionCustomArea = false;
    bool instructionEffects = false;
    uint16_t instructionOptionIds[3] = {};
    bool instructionAnimatedFeedback = false;
#if ENABLE_INSTRUCTION_RESPONSE
    InstructionResponseSettings instructionSettings;
#endif
    uint8_t animationArea[4] = {};
    unsigned instructionOptionGroups = 0;
    unsigned instructionFeedbackDraws = 0;
    uint8_t instructionFeedbackFrames = 1;
    uint8_t instructionFeedbackFrame = 0;
    uint32_t feedbackFirstDrawMs = 0;
    uint32_t feedbackFrameDrawMs = 0;
    uint16_t instructionFrameMs = 167;
    bool pendingFeedbackDelay = false;
    bool commandGuess = false;
    bool missingPredict = false;
    bool navigationButtons = false;
    bool commandSequence = false;
    bool pausedDailyAction = false;
    uint8_t heldAnimationId = 0;
    uint16_t selectedButtonFrame = 0;
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
    uint8_t configuredOutfit = 0;
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
    int statusVersionQueries = 0;
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
    bool catchUpDays(const PetBehaviorConfig &config, const PetBehaviorStatValues &state,
                     const PetBehaviorDailyChangePauses &pauses, uint32_t limit, uint32_t &days) override
    {
        ++scenario->catchUpQueries;
        days = scenario->batchCatchUp ? petBehaviorCatchUpDays(config, state, pauses, limit) : 1;
        if (scenario->batchCatchUp && scenario->catchUpEvents)
            for (int threshold : {99, 95, 93, 90})
                days = petBehaviorRangeChangeDays(config, state, pauses,
                    {runtimeValueIdForPetStat(0), threshold, threshold}, days);
        return scenario->catchUpReadSucceeds;
    }
    void configureRuntimeContract(const PetBehaviorConfig &) override {}
    const char *firstAssetDataErrorResource() const override { return "appearance"; }
    EvolutionLookupResult findEvolutionTarget(const PetStatSnapshot &stats,
                                             AppearanceSelection &selection) override
    {
        ++scenario->evolutionLookups;
        if (scenario->catchUpEvents)
        {
            if ((stats.speciesSlot == 1 && stats.customStats[0] == 93) ||
                (stats.speciesSlot == 2 && stats.customStats[0] == 90))
            {
                selection.speciesSlot = stats.speciesSlot + 1;
                selection.outfitSlot = 1;
                selection.evolutionMode = EvolutionAnimationMode::Disabled;
                return EvolutionLookupResult::Found;
            }
            return EvolutionLookupResult::NoTarget;
        }
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
        if (scenario->catchUpEvents && (stats.customStats[0] == 99 || stats.customStats[0] == 95))
        {
            resolved |= 2;
            if (!scenario->firstUnlockDay) scenario->firstUnlockDay = stats.stage_days;
        }
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
    bool configureActiveAppearance(uint8_t, uint8_t outfit) override
    {
        scenario->appearanceEvents += 'L';
        if (!scenario->reloadSucceeds) return false;
        scenario->configuredOutfit = outfit;
        return true;
    }
};

class SessionHost : public PetSessionHost
{
public:
    SessionHost(PetBehaviorConfig &configRef, AssetResources &resourcesRef,
                AnimationController &animationsRef, AppearanceLoader &loaderRef)
        : config(configRef), resources(resourcesRef), animations(animationsRef), loader(loaderRef) {}
    bool activateLoadedAppearance(uint8_t species, uint8_t outfit) override
    {
        if (!resources.configureBundle(config.assetManifest.bundleId)) return false;
        loader.configureRuntimeContract(config);
        animations.configureRuntimeContract(config);
        return true;
    }
    bool configureActiveAppearance(uint8_t species, uint8_t outfit) override
    {
        return loadRuntimeContract(resources.sdCard(), species, outfit, config) &&
               activateLoadedAppearance(species, outfit);
    }
private:
    PetBehaviorConfig &config;
    AssetResources &resources;
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        AnimationController animations(renderer, resources);
        PetBehaviorConfig config = {};
        PetBehaviorRuntime runtime(config, pet, animations, resources);
        PetSaveController saves(pet, storage, resources);
        SessionHost host(config, resources, animations, loader);
        AppearanceChangeController changes(pet, saves, storage, resources, loader, host);
        PetSession session(pet, storage, resources, loader, config, runtime, changes, host);
        assert(session.state() == PetSession::State::NotPrepared &&
               session.reset() == AppearanceChangeResult::PetStateRejected);
        assert(data.initialLoads == 0 && data.saves == 0);
        assert(session.prepare() && session.state() == PetSession::State::Ready);
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        AnimationController animations(renderer, resources);
        PetBehaviorConfig config = {};
        PetBehaviorRuntime runtime(config, pet, animations, resources);
        PetSaveController saves(pet, storage, resources);
        SessionHost host(config, resources, animations, loader);
        AppearanceChangeController changes(pet, saves, storage, resources, loader, host);
        PetSession session(pet, storage, resources, loader, config, runtime, changes, host);
        assert(!session.prepare() && session.state() == expected[mode]);
        assert(session.errorStage() != nullptr &&
               session.reset() == AppearanceChangeResult::PetStateRejected);
        const int loads = data.initialLoads;
        if (mode != 3)
        {
            assert(!session.prepare());
            assert(data.initialLoads == loads); // Terminal runtime failure.
        }
        else
        {
            data.initialSpecies = 1;
            assert(session.prepare()); // Pet-state failure remains retryable.
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        assert(game.setup_game());
        assert(game.setStageDaysForCheat(1));
        const int savesBefore = data.saves;
        for (uint8_t index = 0; index < sourceCount; ++index)
        {
            assert(pet.speciesSlot() == 1 && data.saves == savesBefore);
            rtcLoop(game);
        }
        assert(data.sourceDraws == sourceCount && pet.speciesSlot() == 2);
        assert(data.saves == savesBefore + 1 && data.targetDraws == 0);
        for (uint8_t index = 0; index < targetCount; ++index)
            rtcLoop(game);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader loader;
    AnimationController animations(renderer, resources);
    EvolutionTestHost host;
    EvolutionController evolution(pet, animations, loader, resources, host);
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
    assert(!animations.takeFullRedrawRequest());
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        PetSaveController saves(pet, storage, resources);
        ChangeHost host;
        AppearanceChangeController changes(pet, saves, storage, resources, loader, host);
        const auto result = changes.changeSpecies(2, failure == 1 ? 0 : 1,
            failure == 5 ? AppearanceChangeController::ContractState::AlreadyActivated
                         : AppearanceChangeController::ContractState::Reload);
        const AppearanceChangeResult expected[] = {
            AppearanceChangeResult::ConfigurationFailed, AppearanceChangeResult::PetStateRejected,
            AppearanceChangeResult::UnlockFailed, AppearanceChangeResult::SaveFailed,
            AppearanceChangeResult::Applied, AppearanceChangeResult::Applied};
        const char *events[] = {"L", "L", "LU", "LUS", "LUS", "US"};
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        PetSaveController saves(pet, storage, resources);
        ChangeHost host;
        AppearanceChangeController changes(pet, saves, storage, resources, loader, host);
        const auto result = changes.applyOutfit(2, failure < 5);
        const AppearanceChangeResult expected[] = {
            AppearanceChangeResult::ConfigurationFailed, AppearanceChangeResult::UnlockFailed,
            AppearanceChangeResult::PetStateRejected, AppearanceChangeResult::SaveFailed,
            AppearanceChangeResult::Applied, AppearanceChangeResult::SaveFailed,
            AppearanceChangeResult::Applied};
        const char *events[] = {"L", "LC", "LC", "LCS", "LCS", "LS", "LS"};
        assert(result == expected[failure]);
        assert(data.appearanceEvents == events[failure]);
        assert(pet.outfitSlot() == (failure < 3 ? 1 : 2));
        assert(pet.stageDays() == (failure == 3 || failure == 4 ? 8 : 10));
        assert(pet.customStat(0) == (failure == 3 || failure == 4 ? 6 : 9));
        assert(pet.outfitUnlockMask() == (failure == 3 || failure == 4 ? 3 : 1));
        assert(data.configuredOutfit == (failure == 0 ? 0 : 2));
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader loader;
    PetSaveController saves(pet, storage, resources);
    ChangeHost host;
    AppearanceChangeController changes(pet, saves, storage, resources, loader, host);
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
    assert(saves.requestSave() == PetSaveResult::Persisted && data.saves == 4);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

    assert(game.prepare_game());
    assert(data.initialLoads == 1 && data.reloads == 0);
    assert(data.unlocks == 1 && data.lastUnlockInitialize);
    assert(data.saves == 1);
    assert(pet.speciesSlot() == 1 && pet.outfitSlot() == 1);
    // Preparation loads state but does not enable input or normal ticking.
    assert(!game.setStageDaysForCheat(1));
    hostMillis = 2000;
    rtcLoop(game);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

    assert(game.setup_game());
    data.reloadSucceeds = false;
    assert(!game.resetPet());
    // The failed reset also attempts to restore the previous appearance contract.
    assert(data.reloads == 2 && data.saves == 1);
    assert(!game.setStageDaysForCheat(1));
    assert(!game.saveNow());
    assert(!game.resetPet());
    assert(data.reloads == 2 && data.saves == 1);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

    assert(game.prepare_game());
    assert(game.finish_setup_game());
    assert(game.setStageDaysForCheat(1));
    if (result == EvolutionLookupResult::LoadFailed)
        hostMillis = 2000;
    rtcLoop(game);

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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);

    assert(game.prepare_game());
    assert(game.finish_setup_game());
    hostMillis = 2000;
    rtcLoop(game);
    assert(data.evolutionLookups == 1);
    assert(strcmp(data.firstRecordedResource, "appearance") == 0);
    assert(strcmp(data.lastShownResource, "appearance") == 0);
    // The daily transaction commits before its evolution lookup fails.
    assert(pet.stageDays() == 1);
    rtcLoop(game);
    assert(strcmp(data.lastShownResource, "appearance") == 0);
    assert(pet.stageDays() == 1); // Fatal state blocks further progression.
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
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
    rtcLoop(game);
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
    rtcLoop(game);
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
            rtcLoop(game);
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
            rtcLoop(game);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    assert(game.setup_game() && game.setStageDaysForCheat(1));
    rtcLoop(game);
    if (afterCommit) rtcLoop(game);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
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
    PetBehaviorRuntime runtime(config, pet, animations, resources);
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

void testUserActionPlaybackSequence()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 0, 0, 100, 0};
    config.idleAnimation = {1, 1, 5};
    config.actions[0].active = true;
    config.actions[0].outcomeCount = 1;
    config.actions[0].followingAnimationCount = 3;
    config.actionOutcomeCount = 1;
    config.actionEffectCount = 1;
    config.actionOutcomes[0].animationPlayback = {{1, 1, 1}, 2};
    config.actionOutcomes[0].effectCount = 1;
    config.actionEffects[0] = {0, PetBehaviorEffectOperation::Change, 10};
    config.followingAnimations[0] = {{1, 1, 2}, 1};
    config.followingAnimations[1] = {{1, 1, 3}, 2};
    config.followingAnimations[2] = {{1, 1, 4}, 1};
    animations.setup(config.idleAnimation);
    PetBehaviorRuntime runtime(config, pet, animations, resources);
    runtime.initializeStats();
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
    assert(pet.customStat(0) == 10);
    // This fake renderer finishes one repetition per tick. No idle segment
    // may be selected until every requested repetition has completed.
    const uint8_t expected[] = {1, 1, 2, 3, 3, 4, 5};
    unsigned long now = 1;
    for (uint8_t id : expected)
    {
        animations.tick(now++);
        assert(data.displayedAnimation.animationId == id);
        assert(pet.customStat(0) == 10);
    }
    assert(!animations.isBusy());

    // Resolve the whole sequence before replacement; missing later resources
    // retain committed effects without starting a partial sequence.
    data.statusVersions = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 20 && !animations.isBusy());
    data.statusVersions = 1;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
    animations.tick(now++);
    animations.cancelAll();
    animations.tick(now++);
    assert(data.displayedAnimation.animationId == 5 && pet.customStat(0) == 30);
}

void testCommandReplacesSequenceAcrossDayBoundary()
{
    for (bool secondSegment : {false, true})
    for (bool dayDue : {false, true})
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandAction = data.navigationButtons = data.commandSequence = true;
        data.heldAnimationId = secondSegment ? 2 : 1;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        Game game(pet, storage, renderer, resources, loader);
        game.synchronizeTime(10);
        assert(game.setup_game());
        game.OnConfirmKey();
        game.loop_game();
        if (secondSegment) game.loop_game();
        assert(data.displayedAnimation.animationId == data.heldAnimationId);
        assert(game.hasTransientAnimation());
        game.synchronizeTime(dayDue ? 12 : 11);
        assert(!game.hasPendingTime());
        game.OnRightKey();
        game.loop_game();
        assert(data.selectedButtonFrame == 3);
        game.OnConfirmKey();
        // The public input route must replace playback even when the current
        // sequence is postponing a day boundary. Effects run exactly once.
        assert(pet.customStat(0) == 4);
        game.loop_game();
        assert(data.displayedAnimation.animationId == 4);
        game.loop_game();
        assert(data.displayedAnimation.animationId == 5);
        assert(!game.hasTransientAnimation()); // Old following segments are gone.
        game.synchronizeTime(dayDue ? 12 : 11);
        assert(!game.hasPendingTime());
        assert(pet.stageDays() == 0);
    }
}

void testActionPlaybackPausesPetTime()
{
    for (uint8_t heldSegment : {1, 2})
    for (bool cancelForSleep : {false, true})
    {
        hostMillis = 1;
        Scenario data;
        scenario = &data;
        data.commandAction = data.commandSequence = data.pausedDailyAction = true;
        data.heldAnimationId = heldSegment;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        Game game(pet, storage, renderer, resources, loader);
        game.synchronizeTime(10);
        assert(game.setup_game());
        game.synchronizeTime(11); // Preserve the second before the action.
        game.OnConfirmKey();
        ++hostMillis;
        game.loop_game();
        if (heldSegment == 2) { ++hostMillis; game.loop_game(); }
        assert(data.displayedAnimation.animationId == heldSegment);
        game.synchronizeTime(16);
        assert(pet.stageDays() == 0 && pet.customStat(0) == 8);
        assert(!game.hasPendingTime());
        if (cancelForSleep)
            game.stopPlaybackForSleep();
        else
        {
            data.heldAnimationId = 0;
            for (int frame = 0; frame < 4; ++frame) { hostMillis += 700; game.loop_game(); }
            assert(!game.hasTransientAnimation());
        }
        game.synchronizeTime(16);
        assert(pet.stageDays() == 0 && pet.customStat(0) == 8);
        game.synchronizeTime(17);
        assert(pet.stageDays() == 1 && pet.customStat(0) == 8); // First pause day.
        game.synchronizeTime(19);
        assert(pet.stageDays() == 2 && pet.customStat(0) == 7);
        game.stopPlaybackForSleep();
        // No playback or clock polls while sleeping; all three days still settle.
        for (int day = 0; day < 3; ++day) game.synchronizeTime(25);
        assert(pet.stageDays() == 5 && pet.customStat(0) == 4);
    }
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        Game game(pet, storage, renderer, resources, loader);
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
        // Catalog builds commit the completed action immediately.
        assert(data.saves == saves + ENABLE_COLLECTION_CATALOG);
        assert(game.hasTransientAnimation() == !missing);
    }
}

void testUnifiedCommandExecution()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
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
    PetBehaviorRuntime runtime(config, pet, animations, resources);
    runtime.initializeStats();
    CommandExecutor executor(pet, animations, runtime);
    executor.configureRuntimeContract(config);
    CommandController commands;
    commands.configure(config);
    // Selection starts on an invisible slot; it cannot apply effects.
    assert(executor.execute(commands).kind == CommandResultKind::None);
    assert(pet.customStat(0) == 2);
    commands.resetSelection();
    assert(commands.selectedSlot() == 3);
    const CommandResult applied = executor.execute(commands);
    assert(applied.kind == CommandResultKind::UserAction);
    assert(!applied.resourceError);
    assert(pet.customStat(0) == 8 && animations.isBusy());
    assert(runtime.advancePetDay() && pet.customStat(0) == 8);
    assert(runtime.advancePetDay() && pet.customStat(0) == 7);
    animations.cancelAll();
    data.actionVersions = 0;
    const CommandResult missing = executor.execute(commands);
    assert(missing.kind == CommandResultKind::UserAction && missing.resourceError);
    assert(pet.customStat(0) == 8 && !animations.isBusy());
    assert(runtime.advancePetDay() && pet.customStat(0) == 8);
    commands.next();
    const CommandResult rejected = executor.execute(commands);
    assert(rejected.kind == CommandResultKind::None && !rejected.resourceError);
    assert(pet.customStat(0) == 8);
    // A subsequent System Command starts with a fresh result.
    commands.next();
    const CommandResult status = executor.execute(commands);
    assert(status.kind == CommandResultKind::Playback && status.resourceError);
    config.buttons[5].active = false;
    commands.configure(config);
    assert(executor.execute(commands).kind == CommandResultKind::None);
    assert(data.saves == 0);
}

void testStatusReadsActionCommittedPet()
{
    Scenario data;
    scenario = &data;
    data.statusVersions = 2;
    SdFat sd;
    Pet pet;
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    AnimationController animations(renderer, resources);
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
    PetBehaviorRuntime runtime(config, pet, animations, resources);
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
    assert(data.statusVersionQueries == 0);
    animations.preparePlayback(0);
    assert(animations.currentVersionIndex() == 0);

    data.actionVersions = 0;
    assert(runtime.executeAction(0) == PetBehaviorActionResult::AppliedAnimationMissing);
    assert(pet.customStat(0) == 8);
    assert(!executor.execute(commands).resourceError);
    assert(data.statusVersionQueries == 0);
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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
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
            rtcLoop(game);
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
            rtcLoop(game);
            game.OnConfirmKey();
            assert(pet.customStat(0) == 8);
            // Restore keeps the completion record, while reset starts a new life.
            Game restored(pet, storage, renderer, resources, appearance);
            assert(restored.prepare_game() && restored.finish_setup_game());
            assert(pet.isFirstStartCompleted());
            assert(!restored.startStartupAnimation());
            const int completedSaves = data.saves;
            assert(restored.resetPet());
            assert(!pet.isFirstStartCompleted());
            rtcLoop(restored);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
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
    rtcLoop(game);
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

#if ENABLE_COMMAND_OUTFIT
void testForegroundInputOwnership()
{
    for (uint8_t mode = 0; mode < 4; ++mode)
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandOutfit = true;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        assert(game.setup_game());
        game.OnConfirmKey();
        int previews = data.previewQueries;
        game.OnRightKey();
        assert(data.previewQueries == previews + 1);
        if (mode == 0)
        {
            game.startBatteryAnimation();
            previews = data.previewQueries;
            const uint8_t outfit = pet.outfitSlot();
            game.OnRightKey();
            game.OnConfirmKey();
            assert(data.previewQueries == previews && pet.outfitSlot() == outfit);
            game.endBatteryAnimation();
            game.OnLeftKey();
            assert(data.previewQueries == previews + 1);
        }
        else if (mode == 1)
        {
            game.startStartupAnimation();
            previews = data.previewQueries;
            game.OnLeftKey();
            game.OnConfirmKey();
            assert(data.previewQueries == previews);
            for (uint8_t tick = 0; tick < 20 && game.hasTransientAnimation(); ++tick)
            {
                hostMillis += 1000;
                game.loop_game();
            }
            game.loop_game();
            game.OnConfirmKey();
            assert(data.previewQueries > previews); // A new chooser, no stale owner.
        }
        else
        {
            // Evolution owns input and closes the old-species chooser; battery returns to commands.
            data.evolutionResult = EvolutionLookupResult::Found;
            data.evolutionMode = mode == 2 ? EvolutionAnimationMode::TwoPhase : EvolutionAnimationMode::Disabled;
            game.synchronizeTime(2);
            previews = data.previewQueries;
            if (mode == 3)
            {
                assert(pet.speciesSlot() == 2);
                game.OnLeftKey();
                assert(data.previewQueries == previews);
                game.OnConfirmKey();
                assert(data.previewQueries > previews);
                continue;
            }
            game.OnLeftKey();
            game.OnConfirmKey();
            assert(data.previewQueries == previews && pet.speciesSlot() == 1);
            game.startBatteryAnimation();
            game.endBatteryAnimation();
            game.OnLeftKey();
            assert(data.previewQueries == previews);
            game.OnConfirmKey();
            assert(data.previewQueries > previews); // Reopens with current appearance data.
        }
    }
    puts("[PASS] One foreground input owner: chooser/battery/startup/evolution interruption and return");
}
#endif

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
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
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
            resources.recordAssetDataErrorResource("shared.data");
            rtcLoop(game);
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
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    assert(game.prepare_game());
    assert(data.resourceShows == 0);
    data.layoutSucceeds = false;
    assert(!game.finish_setup_game());
    assert(data.resourceShows == 1);
    data.layoutSucceeds = true;
    assertFatalBlocksPublicEntries(game, pet, data);
}

#if ENABLE_SEQUENCE_AUDIO
void testIdleMusicGameLifecycle()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    data.idleMusic = true;
    data.deathEnabled = bool(ENABLE_DEATH);
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    game.synchronizeTime(10);
    assert(game.setup_game());
    game.loop_game();
    assert(game.audioActive() && !game.hasTransientAnimation() && !game.preventsIdleSleep());
    hostMillis = 2000;
    game.synchronizeTime(12);
    game.loop_game();
    assert(pet.stageDays() == 1 && game.audioActive());
    game.stopPlaybackForSleep();
    assert(!game.audioActive());
    hostMillis = 3000;
    game.redrawAllNow();
    assert(game.audioActive());
    game.synchronizeTime(2012); // 1000 RTC days can settle while base music loops.
    for (uint32_t pending = 0; game.hasPendingTime() && pending < 1000; ++pending)
        game.synchronizeTime(2012);
    assert(pet.stageDays() == 1001 && !game.hasPendingTime());
    game.OnConfirmKey();
    assert(pet.customStat(0) == 8 && !game.audioActive());
    game.loop_game();
    hostMillis += 100;
    game.loop_game();
    assert(game.audioActive() && !game.hasTransientAnimation());
#if ENABLE_DEATH
    assert(pet.setCustomStat(0, 0));
    game.loop_game();
    assert(game.isDead() && !game.audioActive());
    game.redrawAllNow();
    assert(!game.audioActive());
#endif
#if ENABLE_APPEARANCE_SELECTION
    hostMillis = 0;
    Scenario chooser;
    scenario = &chooser;
    chooser.commandAction = chooser.commandOutfit = chooser.idleMusic = true;
    Pet menuPet;
    PetStorage menuStorage(&sd);
    AssetResources menuRendererResources(&sd);
    Renderer menuRenderer(nullptr, menuRendererResources);
    Game menuGame(menuPet, menuStorage, menuRenderer, menuRendererResources, appearance);
    assert(menuGame.setup_game());
    menuGame.loop_game();
    assert(menuGame.audioActive());
    menuGame.OnConfirmKey();
    assert(!menuGame.audioActive());
    menuGame.redrawAllNow();
    assert(!menuGame.audioActive());
#endif
}
#endif

void testRtcCatchUp()
{
    for (int failure = 0; failure < 3; ++failure)
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandAction = true;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        game.synchronizeTime(10);
        assert(game.setup_game());
        const int savesBefore = data.saves;
        data.saveSucceeds = failure != 1;
        data.unlockSucceeds = failure != 2;
        game.synchronizeTime(3611); // 1800 days plus one second of remainder.
        assert(pet.stageDays() == 1 && game.hasPendingTime());
        game.OnConfirmKey();
        assert(pet.customStat(0) == 2); // Historical days precede feeding.
        for (int day = 1; day < 1800; ++day)
        {
            game.synchronizeTime(3611);
            game.loop_game();
        }
        assert(pet.stageDays() == 1800 && !game.hasPendingTime());
#if ENABLE_COLLECTION_CATALOG
        // Failed unlock evaluation never reaches a complete daily mutation.
        // Do not commit collection progress until that evaluation succeeds.
        assert(data.saves == savesBefore + (failure == 2 ? 0 : 1));
#else
        assert(data.saves == savesBefore + 1); // One periodic batch write.
#endif
        game.synchronizeTime(3611);
        assert(pet.stageDays() == 1800);
        game.synchronizeTime(3612);
        assert(pet.stageDays() == 1801); // Preserve the partial day.
        if (failure == 1) assert(data.saves == savesBefore + 2);
        if (failure == 0)
        {
            game.OnConfirmKey();
            assert(pet.customStat(0) == 8);
            game.synchronizeTime(3614);
            assert(pet.stageDays() == 1801 && !game.hasPendingTime());
            game.loop_game();
            game.synchronizeTime(3614);
            assert(pet.stageDays() == 1801); // Playback time is excluded.
        }
    }
}

void testRtcSnapshotAndReset()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    game.synchronizeTime(10);
    assert(game.setup_game());
    game.synchronizeTime(14);
    assert(pet.stageDays() == 1 && game.hasPendingTime());
    game.synchronizeTime(18); // Catch-up itself crossed two more boundaries.
    assert(pet.stageDays() == 2 && game.hasPendingTime());
    game.OnConfirmKey();
    assert(pet.customStat(0) == 2);
    game.synchronizeTime(18);
    game.synchronizeTime(18);
    assert(pet.stageDays() == 4 && !game.hasPendingTime());
    game.redrawAllNow();
    game.synchronizeTime(19);
    assert(pet.stageDays() == 4); // Redraw does not reset the partial day.
    game.synchronizeTime(20);
    assert(pet.stageDays() == 5);
    assert(game.resetPet());
    game.synchronizeTime(21);
    assert(pet.stageDays() == 0);
    game.synchronizeTime(22);
#if !ENABLE_STARTUP_ANIMATION
    assert(pet.stageDays() == 1);
#endif
}

void testNavigationDuringRtcCatchUp()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = data.navigationButtons = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    game.synchronizeTime(10);
    assert(game.setup_game());
    game.synchronizeTime(30);
    assert(game.hasPendingTime());
    game.OnRightKey();
    game.loop_game();
    assert(data.selectedButtonFrame == 3); // Selection paints on this loop, not after catch-up.
    assert(pet.stageDays() == 1 && pet.customStat(0) == 2);
    game.OnLeftKey();
    game.loop_game();
    assert(data.selectedButtonFrame == 2);
}

void testFourSecondPeriodicSave()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    game.synchronizeTime(0);
    assert(game.setup_game());
    const int initialSaves = data.saves;
    for (uint32_t seconds = 1; seconds <= 8; ++seconds)
    {
        hostMillis = seconds * 1000;
        game.synchronizeTime(seconds);
        game.loop_game();
        assert(data.saves == initialSaves + static_cast<int>(seconds / 4));
    }
    assert(pet.stageDays() == 4);
}

void testDailyPostProcessingDisabled()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    game.synchronizeTime(0);
    assert(game.setup_game());
    const int initialSaves = data.saves;
    const int initialUnlocks = data.unlocks;
    const int initialLookups = data.evolutionLookups;
    data.unlockSucceeds = false;
    data.evolutionResult = EvolutionLookupResult::LoadFailed;
    for (uint32_t seconds = 1; seconds <= 8; ++seconds)
    {
        hostMillis = seconds * 1000;
        game.synchronizeTime(seconds);
        game.loop_game();
    }
    assert(pet.stageDays() == 4);
    assert(data.saves == initialSaves && data.unlocks == initialUnlocks);
    assert(data.evolutionLookups == initialLookups && data.resourceShows == 0);
    assert(game.isReady());
    data.unlockSucceeds = true;
    game.OnConfirmKey();
    assert(pet.customStat(0) == 8);
    assert(data.saves == initialSaves + ENABLE_COLLECTION_CATALOG);
}

void testDailySaveRetry()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    PetSaveController saves(pet, storage, resources);
    assert(saves.saveDailyProgress() == PetSaveResult::NotNeeded && data.saves == 0);
    data.saveSucceeds = false;
    assert(saves.saveDailyProgress() == PetSaveResult::Failed && data.saves == 1);
    data.saveSucceeds = true;
    // Retry on the next day; a failed write must not reset the interval.
    assert(saves.saveDailyProgress() == PetSaveResult::Persisted && data.saves == 2);
    assert(saves.saveDailyProgress() == PetSaveResult::NotNeeded && data.saves == 2);
    assert(saves.saveDailyProgress() == PetSaveResult::Persisted && data.saves == 3);
    data.saveSucceeds = false;
    assert(saves.requestSave() == PetSaveResult::Failed && data.saves == 4);
    data.saveSucceeds = true;
    // Pending event commits retry even before the ordinary two-day threshold.
    assert(saves.saveDailyProgress(true) == PetSaveResult::Persisted && data.saves == 5);
}

void testSaveRequestResults()
{
    Scenario data;
    scenario = &data;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    PetSaveController saves(pet, storage, resources);
    assert(saves.endBatch() == PetSaveResult::NotNeeded && data.saves == 0);
    assert(saves.maybeSave() == PetSaveResult::NotNeeded && data.saves == 0);

    saves.beginBatch();
    assert(saves.maybeSave() == PetSaveResult::Deferred && data.saves == 0);
    assert(saves.requestSave() == PetSaveResult::Deferred && data.saves == 0);
    pet.setStageDays(7); // The batch writes its final state, not the request-time snapshot.
    assert(saves.endBatch() == PetSaveResult::Persisted && data.saves == 1);
    assert(data.saveInput.stage_days == 7);
    assert(saves.endBatch() == PetSaveResult::NotNeeded && data.saves == 1);

    saves.beginBatch();
    assert(saves.saveDailyProgress() == PetSaveResult::Deferred && data.saves == 1);
    // Life-state commits bypass batching and clear its pending snapshot.
    assert(saves.saveLifeState() == PetSaveResult::Persisted && data.saves == 2);
    assert(saves.endBatch() == PetSaveResult::NotNeeded && data.saves == 2);

    saves.beginBatch();
    assert(saves.requestSave() == PetSaveResult::Deferred && data.saves == 2);
    data.saveSucceeds = false;
    assert(saves.endBatch() == PetSaveResult::Failed && data.saves == 3);
    assert(saves.endBatch() == PetSaveResult::NotNeeded && data.saves == 3);
    data.saveSucceeds = true;
    assert(saves.requestSave() == PetSaveResult::Persisted && data.saves == 4);

    saves.beginBatch();
    assert(saves.requestSave() == PetSaveResult::Deferred && data.saves == 4);
    // Immediate collection events retain their existing bypass policy too.
    assert(saves.saveCollectionProgress(true, true) == PetSaveResult::Persisted && data.saves == 5);
    assert(saves.endBatch() == PetSaveResult::NotNeeded && data.saves == 5);
}
void testDeathCompileFlag()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    data.deathEnabled = bool(ENABLE_DEATH);
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader loader;
    Game game(pet, storage, renderer, resources, loader);
    assert(game.setup_game());
    assert(pet.setCustomStat(0, 0));
    game.loop_game();
    assert(game.isDead() == bool(ENABLE_DEATH));
    if (ENABLE_DEATH)
    {
        game.OnDeathKey();
        assert(!game.isDead() && pet.customStat(0) == 2);
    }
}

void testSavedDeathReset()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    data.deathEnabled = bool(ENABLE_DEATH);
    data.hasSave = true;
    PersistedPetState saved = data.savedPet.persistentState();
    saved.schemaFingerprint = 123;
    saved.flowFlags |= 0x4UL;
    saved.customStats[0] = 2;
    assert(data.savedPet.restoreState(saved));
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader loader;
    Game game(pet, storage, renderer, resources, loader);
    assert(game.setup_game() && game.isDead());
    assert(!pet.setCustomStat(0, 5));
    data.saveSucceeds = false;
    game.OnDeathKey();
    assert(game.isReady() && game.isDead());
    data.saveSucceeds = true;
    game.OnDeathKey();
    assert(game.isReady() && !game.isDead() && !data.savedPet.isDead());
    assert(pet.customStat(0) == 2);
    assert(pet.setCustomStat(0, 0));
    game.loop_game();
    assert(game.isDead() == bool(ENABLE_DEATH));
}

void testBatchedCatchUpMatchesDailyEvents()
{
    PetStatSnapshot reference = {};
    bool referenceDead = false;
    int referenceSteps = 0;
    for (bool batch : {false, true})
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandAction = true;
        data.catchUpEvents = true;
        data.batchCatchUp = batch;
        data.deathEnabled = bool(ENABLE_DEATH);
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader loader;
        Game game(pet, storage, renderer, resources, loader);
        assert(game.setup_game());
        game.synchronizeTime(0);
        int steps = 0;
        do
        {
            game.synchronizeTime(2001); // 1000 days plus a retained partial day.
            game.loop_game();
            assert(++steps < 1100);
        } while (game.hasPendingTime() && !game.isDead());
        assert(game.isReady());
        assert(data.firstUnlockDay == 1); // Already matching before the first batch.
#if ENABLE_DAILY_POST_PROCESSING
        assert(pet.speciesSlot() == 3); // Both one-day evolution windows survived.
#endif
        if (!batch)
        {
            reference = pet.statSnapshot();
            referenceDead = pet.isDead();
            referenceSteps = steps;
        }
        else
        {
            const auto actual = pet.statSnapshot();
            assert(actual.stage_days == reference.stage_days && actual.speciesSlot == reference.speciesSlot);
            assert(actual.customStats[0] == reference.customStats[0] && pet.isDead() == referenceDead);
            assert(steps < 20 && steps < referenceSteps);
            if (!pet.isDead())
            {
                const uint32_t days = pet.stageDays();
                game.synchronizeTime(2002);
                assert(pet.stageDays() == days + 1); // Batch retained the one-second remainder.
            }
        }
    }
    // A failed boundary read cannot consume time or mutate the pet.
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = true;
    data.batchCatchUp = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader loader;
    Game game(pet, storage, renderer, resources, loader);
    assert(game.setup_game());
    game.synchronizeTime(0);
    game.synchronizeTime(1000); // First day settles existing matches.
    const auto before = pet.statSnapshot();
    data.catchUpReadSucceeds = false;
    game.synchronizeTime(1000);
    assert(!game.isReady());
    assert(pet.stageDays() == before.stage_days && pet.customStat(0) == before.customStats[0]);
}
} // namespace

#if ENABLE_COLLECTION_CATALOG
// No collection conditions in these fixtures. Exercise the production Game
// and save controller with catalog support without requiring SD artwork.
bool evaluateCollectionCatalog(SdFat *, const PetBehaviorConfig &, Pet &, bool &changed)
{
    changed = false;
    return true;
}
bool applyCollectionOutfitGrants(SdFat *, const PetBehaviorConfig &, Pet &) { return true; }
CollectionCatalog::CollectionCatalog(Pet &p, Renderer &r, AssetResources &a, const PetBehaviorConfig &c)
    : pet(p), renderer(r), resources(a), config(c) {}
bool CollectionCatalog::start() { return false; }
bool CollectionCatalog::move(bool) { return false; }
bool CollectionCatalog::confirm() { return false; }
bool CollectionCatalog::redraw() { return false; }
bool CollectionCatalog::takeActivation(uint8_t &, uint8_t &) { return false; }
#endif

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
#if ENABLE_SEQUENCE_AUDIO
    if (scenario->idleMusic) config.animationAudio[config.idleAnimation.animationId - 1] = {0, 16000};
#endif
    config.deathEnabled = scenario->deathEnabled;
    config.deathStatMask = 1;
    config.deathAnimation = {selection.speciesSlot, selection.outfitSlot, 9};
    config.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::FirstStart)] =
        {selection.speciesSlot, selection.outfitSlot, 6};
    if (scenario->commandAction)
    {
        config.stats[0] = {true, 2, 0, 10, 0};
        if (scenario->pausedDailyAction) config.stats[0].dailyChange = -1;
        if (scenario->catchUpEvents) config.stats[0] = {true, 100, 0, 100, -1};
        config.actions[0].active = true;
        if (scenario->pausedDailyAction) config.actions[0].suspendDailyChangeDays = 1;
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
    if (scenario->navigationButtons)
    {
        config.buttons[1] = config.buttons[0];
        config.screenBlockCount = 2;
        config.screenBlocks[1] = config.screenBlocks[0];
        config.screenBlocks[1].source = 2;
        config.layoutUnselected = {1, 1, 7};
        config.layoutSelected = {1, 1, 8};
    }
    if (scenario->commandSequence)
    {
        config.actions[0].followingAnimationCount = 2;
        config.followingAnimations[0] = {{1, 1, 2}, 1};
        config.followingAnimations[1] = {{1, 1, 3}, 1};
        config.actions[1].active = true;
        config.actions[1].firstOutcome = 1;
        config.actions[1].outcomeCount = 1;
        config.actionOutcomeCount = 2;
        config.actionOutcomes[1].animationPlayback = {{1, 1, 4}, 1};
        config.actionOutcomes[1].firstEffect = 1;
        config.actionOutcomes[1].effectCount = 1;
        config.actionEffectCount = 2;
        config.actionEffects[1] = {0, PetBehaviorEffectOperation::Set, 4};
        config.buttons[1].actionSlot = 1;
    }
#if ENABLE_INSTRUCTION_RESPONSE
    if (scenario->commandInstruction)
    {
        config.instructionSettings = scenario->instructionSettings;
        config.buttons[0] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::InstructionResponse};
        if (scenario->instructionEffects)
        {
            config.stats[0] = {true, 9, 0, 10, 0};
            config.instructionEffectCount = 2;
            config.instructionEffects[0] = {0, 0, PetBehaviorEffectOperation::Change, 5};
            config.instructionEffects[1] = {1, 0, scenario->deathEnabled ? PetBehaviorEffectOperation::Set : PetBehaviorEffectOperation::Change,
                                            scenario->deathEnabled ? 0 : -5};
        }
        if (scenario->instructionCustomArea)
        {
            auto &area = config.screenBlocks[config.screenBlockCount++];
            area.kind = ScreenBlockKind::Animation;
            area.x = 16; area.y = 32; area.width = 96; area.height = 96;
        }
        if (scenario->instructionAnimatedFeedback)
        {
            config.systemAnimations[38] = {1, 1, 95};
            config.systemAnimations[39] = {1, 1, 95};
        }
        for (uint8_t item = 0; item < 3; ++item)
        {
            config.instructionOptions[item] = {0, 0, static_cast<uint16_t>(80 + item)};
            config.systemAnimations[35 + item] = {1, 1, static_cast<uint16_t>(90 + item)};
            if (config.instructionSettings.noGoEnabled)
                config.systemAnimations[40 + item] = {1, 1, static_cast<uint16_t>(85 + item)};
        }
    }
#endif
    if (scenario->commandPredict)
    {
        config.buttons[0] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::Predict};
        config.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::PredAnim)] = {1, 1, 10};
        config.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::Battery)] = {1, 1, 22};
        for (size_t index = 0; index < 11; ++index)
            config.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::Predict1) + index] =
                {1, 1, static_cast<uint16_t>(11 + index)};
        if (scenario->missingPredict)
            config.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::Predict11)] = {};
    }
#if ENABLE_GUESS_GAME
    if (scenario->commandGuess)
    {
        config.buttons[0] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::GuessGame};
        config.buttons[1] = {true, PetBehaviorButtonKind::SystemCommand, 0, RuntimeSystemCommandId::Predict};
        for (auto role : {FirmwarePlaybackRole::GuessStart, FirmwarePlaybackRole::GuessItem1,
                         FirmwarePlaybackRole::GuessItem2, FirmwarePlaybackRole::GuessItem3,
                         FirmwarePlaybackRole::GuessItem4, FirmwarePlaybackRole::GuessLL,
                         FirmwarePlaybackRole::GuessLR, FirmwarePlaybackRole::GuessRL,
                         FirmwarePlaybackRole::GuessRR, FirmwarePlaybackRole::GuessRight,
                         FirmwarePlaybackRole::GuessWrong, FirmwarePlaybackRole::GuessWin,
                         FirmwarePlaybackRole::GuessLoss})
            config.systemAnimations[static_cast<size_t>(role)] = {1, 1, static_cast<uint16_t>(40 + static_cast<uint8_t>(role))};
        config.guessEffectCount = 1;
        config.guessEffects[0] = {true, PetBehaviorGuessOutcome::RoundCorrect, 0, PetBehaviorEffectOperation::Change, 1};
    }
#endif
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

AssetResources::AssetResources(SdFat *sd)
    : sd_(sd), reader_(sd, readBuffer_, AssetData::kIoScratchBytes) {}
BundleReader::BundleReader(SdFat *sd, uint8_t *scratch, size_t size)
    : sd_(sd), scratch_(scratch), scratchSize_(size) {}

Renderer::Renderer(Adafruit_ST7735 *display, AssetResources &resourceRef)
    : tft(display), resources(resourceRef), state(nullptr) {}
Renderer::~Renderer() = default;
void Renderer::initAnimations() {}
void Renderer::setAnimationArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    scenario->animationArea[0] = x; scenario->animationArea[1] = y;
    scenario->animationArea[2] = width; scenario->animationArea[3] = height;
}
bool AssetData::sameBundleId(const BundleId &left, const BundleId &right)
{
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}
bool AssetResources::configureBundle(const AssetData::BundleId &) { return scenario->bundleSucceeds; }
bool Renderer::setAnimation(const AssetData::AnimationRef &animation, uint8_t, bool once)
{
    scenario->displayedAnimation = animation;
    if (animation.animationId == 95)
    {
        assert(once);
        scenario->pendingFeedbackDelay = true;
        scenario->instructionFeedbackFrame = 0;
    }
    return true;
}
bool Renderer::currentLayoutId(uint8_t &layout) const { layout = 0; return scenario->layoutSucceeds; }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &animation, uint8_t,
                                  uint16_t frame, int x, int, int, uint16_t, uint16_t)
{
    if (animation.animationId >= 80 && animation.animationId <= 82)
        scenario->instructionOptionIds[x / 43] = animation.animationId;
    if (animation.animationId == 8) scenario->selectedButtonFrame = frame;
    ++scenario->playbackDraws;
    return true;
}
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::advanceAnimationFrame()
{
    const uint8_t id = scenario->displayedAnimation.animationId;
    if (id == 95 && scenario->instructionFeedbackFrame >= scenario->instructionFeedbackFrames)
        return true;
    ++scenario->playbackDraws;
    if (id == 95) ++scenario->instructionFeedbackFrame;
    if (id == 95 && scenario->pendingFeedbackDelay)
    {
        scenario->pendingFeedbackDelay = false;
        ++scenario->instructionFeedbackDraws;
        hostMillis += scenario->feedbackFirstDrawMs;
    }
    else if (id == 95)
        hostMillis += scenario->feedbackFrameDrawMs;
    if (id == 3) ++scenario->sourceDraws;
    if (id == 4) ++scenario->targetDraws;
    scenario->frameFailed = id != 0 && id == scenario->failedAnimationId;
    if (scenario->frameFailed && scenario->evolutionMode != EvolutionAnimationMode::Disabled &&
        scenario->assetError)
        resources.recordAssetDataErrorResource(id == 3 ? "source.data" : "target.data");
    if (id == 95) return scenario->frameFailed;
#if ENABLE_GUESS_GAME
    // A looping item prompt does not finish after the fake display's first frame.
    if (scenario->commandGuess &&
        (id == 40 + static_cast<uint8_t>(FirmwarePlaybackRole::GuessItem1) ||
         id == 40 + static_cast<uint8_t>(FirmwarePlaybackRole::GuessItem2) ||
         id == 40 + static_cast<uint8_t>(FirmwarePlaybackRole::GuessItem3) ||
         id == 40 + static_cast<uint8_t>(FirmwarePlaybackRole::GuessItem4)))
        return false;
#endif
    return scenario->heldAnimationId == 0 || id != scenario->heldAnimationId;
}
bool Renderer::animationFrameFailed() const { return scenario->frameFailed; }
uint16_t AssetResources::frameCountFor(const AssetData::AnimationRef &animation, uint8_t)
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
uint16_t AssetResources::versionCountFor(const AssetData::AnimationRef &animation)
{
    if (animation.animationId == 2) ++scenario->statusVersionQueries;
    return animation.animationId == 1 ? scenario->actionVersions
         : animation.animationId == 2 ? scenario->statusVersions : 1;
}
unsigned long AssetResources::frameIntervalFor(const AssetData::AnimationRef &, uint8_t,
                                         unsigned long defaultInterval)
{
    return scenario->commandInstruction ? scenario->instructionFrameMs : defaultInterval;
}
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
void AssetResources::recordAssetDataErrorResource(const char *resource)
{
    if (scenario->firstRecordedResource[0] == '\0')
        copyName(scenario->firstRecordedResource, sizeof(scenario->firstRecordedResource), resource);
}
AssetData::BundleError AssetResources::firstAssetDataError() const
{
    return scenario->assetError && scenario->frameFailed
               ? static_cast<AssetData::BundleError>(1) : AssetData::BundleError::None;
}
const char *AssetResources::firstAssetDataErrorResource() const { return scenario->firstRecordedResource; }

#if ENABLE_COMMAND_PREDICT
void testPredictMinigame()
{
    for (int mode = 0; mode < 6 + ENABLE_DEATH; ++mode)
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandAction = data.commandPredict = data.navigationButtons = true;
        data.heldAnimationId = 10;
        data.missingPredict = mode == 1;
        data.deathEnabled = mode == 6;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        game.synchronizeTime(10);
        assert(game.setup_game());
        game.OnConfirmKey();
        if (data.missingPredict)
        {
            assert(!game.hasTransientAnimation());
            game.OnRightKey();
            game.OnConfirmKey();
            assert(pet.customStat(0) == 8); // Failed availability leaves the command menu usable.
            continue;
        }
        game.loop_game();
        assert(data.displayedAnimation.animationId == 10 && game.preventsIdleSleep());
        game.OnRightKey();
        game.OnConfirmKey();
        assert(pet.customStat(0) == 2); // Minigame input cannot execute a different menu action.
        game.synchronizeTime(20);
        assert(pet.stageDays() == 0 && !game.hasPendingTime()); // Preserve excluded Predict playback time.
        if (mode == 2)
        {
            data.failedAnimationId = 10;
            hostMillis = 700;
            game.loop_game();
            assert(data.resourceShows == 1 && !game.hasTransientAnimation());
            data.failedAnimationId = 0;
            game.loop_game(); // Failed game returns to commands.
        }
        else if (mode == 3)
        {
            assert(game.resetPet());
            // New-life reset retains the configured FirstStart sequence, not Predict.
            assert(game.hasTransientAnimation() == bool(ENABLE_STARTUP_ANIMATION && ENABLE_FIRST_START_ANIMATION));
            for (uint8_t tick = 0; tick < 20 && game.hasTransientAnimation(); ++tick)
            {
                hostMillis += 1000;
                game.loop_game();
            }
            game.loop_game();
            assert(!game.hasTransientAnimation());
        }
        else if (mode == 4)
        {
            game.startBatteryAnimation();
            game.endBatteryAnimation();
            game.loop_game();
            assert(!game.hasTransientAnimation());
        }
        else if (mode == 5)
        {
            game.stopPlaybackForSleep();
            game.redrawAllNow();
            game.loop_game();
            assert(!game.hasTransientAnimation());
        }
#if ENABLE_DEATH
        else if (mode == 6)
        {
            assert(pet.setCustomStat(0, 0));
            game.loop_game();
            assert(game.isDead() && !game.hasTransientAnimation());
            game.OnConfirmKey();
            assert(!game.isDead());
            for (uint8_t tick = 0; tick < 20 && game.hasTransientAnimation(); ++tick)
            {
                hostMillis += 1000;
                game.loop_game();
            }
            game.loop_game();
            assert(!game.hasTransientAnimation());
        }
#endif
        else
        {
            data.heldAnimationId = 0;
            hostMillis = 700;
            game.loop_game(); // Finish the once-only introduction.
            hostMillis = 701;
            game.loop_game(); // Start the selected timed result.
            assert(data.displayedAnimation.animationId >= 11 && data.displayedAnimation.animationId <= 21);
            hostMillis = 5501;
            game.loop_game();
            game.loop_game();
            assert(!game.hasTransientAnimation());
        }
        game.OnRightKey();
        game.OnConfirmKey();
        if (pet.customStat(0) != 8) fprintf(stderr, "Predict route mode=%d transient=%d\n", mode, game.hasTransientAnimation());
        assert(pet.customStat(0) == 8); // Completion, failure and reset all release minigame routing.
    }

    // Preserve the same PRNG choice and all eleven result roles.
    Scenario data;
    scenario = &data;
    data.commandPredict = true;
    SdFat sd;
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    PetBehaviorConfig config = {};
    AppearanceSelection selection;
    bool resolved = false;
    char resource[20] = {};
    assert(loadInitialRuntimeContract(&sd, selection, config, resolved, resource, sizeof(resource)));
    AnimationController animations(renderer, resources);
    animations.configureRuntimeContract(config);
    animations.setup(config.idleAnimation);
    PredictGame predict(animations);
    uint16_t seen = 0;
    for (uint32_t seed : {1U, 33U, 48U, 49U, 66U, 96U, 98U, 99U, 113U, 114U, 180U})
    {
        animations.cancelAll();
        FirmwareRandom::seed(seed);
        const uint16_t expected = FirmwareRandom::below(11);
        FirmwareRandom::seed(seed);
        assert(PredictGame::canStart(animations) && predict.start());
        assert(animations.hasAnimationPending(static_cast<FirmwarePlaybackRole>(
            static_cast<uint8_t>(FirmwarePlaybackRole::Predict1) + expected)));
        seen |= static_cast<uint16_t>(1U << expected);
        predict.onPlaybackFailed();
        assert(!predict.isActive() && !animations.isBusy());
    }
    assert(seen == 0x7ff);
#if ENABLE_SEQUENCE_AUDIO
    // Timed system animations use their own v21 BGM and keep the original
    // 40-second introduction / 4.8-second result across animation loop boundaries.
    for (uint8_t id = 10; id <= 21; ++id) config.animationAudio[id - 1] = {0, 800};
    AnimationController musical(renderer, resources);
    musical.configureRuntimeContract(config);
    musical.setup(config.idleAnimation);
    PredictGame musicalPredict(musical);
    assert(musicalPredict.start());
    auto tick = [&](unsigned long now) {
        hostMillis = now;
        musical.preparePlayback(now);
        musical.tick(now);
        musicalPredict.update();
    };
    tick(1000);
    assert(musical.audioActive() && musical.currentPlaybackRole() == FirmwarePlaybackRole::PredAnim);
    for (unsigned long now = 1600; now <= 40600; now += 600) tick(now);
    tick(40999);
    assert(musicalPredict.isActive() && musical.currentPlaybackRole() == FirmwarePlaybackRole::PredAnim);
    tick(41000);
    assert(musicalPredict.isActive() && musical.currentPlaybackRole() >= FirmwarePlaybackRole::Predict1 &&
           musical.currentPlaybackRole() <= FirmwarePlaybackRole::Predict11);
    for (unsigned long now = 41600; now <= 45200; now += 600) tick(now);
    tick(45799);
    assert(musicalPredict.isActive());
    tick(45800);
    assert(!musicalPredict.isActive() && !musical.audioActive());
#endif
    puts("[PASS] Predict minigame: availability, 11 seeded results, routing, duration, clock pause, failure and reset");
}

#if ENABLE_GUESS_GAME
void testSwitchFromGuessToPredict()
{
    Scenario data;
    scenario = &data;
    data.commandAction = data.commandPredict = data.commandGuess = data.navigationButtons = true;
    hostMillis = 0;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    assert(game.setup_game());
    game.OnConfirmKey();
    const int16_t expectedStat = 2 + (ENABLE_GUESS_GAME_SINGLE_ROUND ? 1 : 3);
    for (unsigned tick = 0; tick < 100; ++tick)
    {
        hostMillis += 700;
        FirmwareRandom::seed(1); // The next item reveal chooses Right.
        game.loop_game();
        if (!game.hasTransientAnimation() && pet.customStat(0) == expectedStat) break;
        game.OnRightKey();
    }
    game.loop_game();
    assert(!game.hasTransientAnimation());
    assert(pet.customStat(0) == expectedStat);
    game.OnRightKey();
    game.OnConfirmKey();
    game.loop_game();
    assert(data.displayedAnimation.animationId == 10 && game.hasTransientAnimation());
    puts("[PASS] Guess outcomes finish through the shared controller, then Predict starts independently");
}
#endif
#endif

#if ENABLE_INSTRUCTION_RESPONSE
void Renderer::clearInstructionOptions() { ++scenario->instructionOptionGroups; }
void Renderer::showInstructionFeedback(bool)
{
    ++scenario->instructionFeedbackDraws;
    hostMillis += scenario->feedbackFirstDrawMs;
}
void testInstructionResetOwnership()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = data.commandInstruction = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    assert(game.setup_game());
    game.OnConfirmKey();
    assert(game.capturesInstructionInput());
    game.loop_game();
    assert(game.resetPet());
    assert(!game.capturesInstructionInput() && pet.customStat(0) == 2);
    for (uint8_t tick = 0; tick < 20 && game.hasTransientAnimation(); ++tick)
    {
        hostMillis += 1000;
        game.loop_game();
    }
    game.loop_game();
    assert(!game.hasTransientAnimation());
    game.OnConfirmKey();
    assert(game.capturesInstructionInput() && pet.customStat(0) == 2);
    puts("[PASS] Reset discards Instruction session, preserves new-life startup, and accepts a fresh session");
}

void testInstructionDeferredEffects()
{
    // Complete, cancelled, precommit read failure, failed save, postcommit visual failure, death.
    for (unsigned kind = 0; kind < 4; ++kind)
    for (unsigned mode = 0; mode < 8; ++mode)
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandAction = data.commandInstruction = data.navigationButtons = data.instructionEffects = true;
        data.instructionSettings.hard = (kind & 1) != 0;
        data.instructionSettings.noGoEnabled = (kind & 2) != 0;
        data.instructionSettings.typeWeights[0] = 0;
        data.instructionSettings.typeWeights[1] = 1;
        data.deathEnabled = mode == 5;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        assert(game.setup_game());
        const int savesBefore = data.saves;
        game.OnConfirmKey();
        game.instructionInput(0, 0, hostMillis);
        game.loop_game();
        for (unsigned round = 0; round < 10; ++round)
        {
            game.instructionInput(0, 0, hostMillis);
            game.loop_game();
            if (mode == 1 && round == 1)
            {
                game.instructionInput(5, 5, hostMillis);
                hostMillis += 1000;
                game.loop_game();
                break;
            }
            if (mode == 2 && round == 1)
            {
                data.failedAnimationId = data.displayedAnimation.animationId;
                hostMillis += 167;
                game.loop_game();
                break;
            }
            if (mode == 6 && round == 1)
            {
                game.startBatteryAnimation();
                assert(!game.capturesInstructionInput());
                game.endBatteryAnimation();
                game.loop_game();
                break;
            }
            if ((round % 2 == 0) != data.instructionSettings.noGoEnabled)
            {
                const uint16_t option = 80 + data.displayedAnimation.animationId - (data.instructionSettings.noGoEnabled ? 85 : 90);
                uint8_t pressed = 0;
                for (uint8_t index = 0; index < 3; ++index)
                    if (data.instructionOptionIds[index] == option) pressed = 1 << index;
                assert(pressed);
                game.instructionInput(pressed, pressed, hostMillis + 1);
                game.instructionInput(0, 0, hostMillis + 2);
            }
            hostMillis += 2000;
            game.loop_game();
            assert(pet.customStat(0) == 9 && data.saves == savesBefore && !pet.isDead());
            hostMillis += 499;
            game.loop_game();
            assert(pet.customStat(0) == 9 && data.saves == savesBefore);
            if (mode == 7 && round == 9)
            {
                const uint32_t pressedAt = hostMillis - 4;
                game.instructionPending(true, pressedAt, 5);
                ++hostMillis;
                game.loop_game();
                assert(game.capturesInstructionInput() && pet.customStat(0) == 9 && data.saves == savesBefore);
                hostMillis = pressedAt + 20;
                game.instructionInput(5, 5, pressedAt);
                game.instructionPending(false, hostMillis, 5);
                game.loop_game();
                hostMillis = pressedAt + 999;
                game.loop_game();
                assert(game.capturesInstructionInput() && pet.customStat(0) == 9);
                ++hostMillis;
                game.loop_game();
                break;
            }
            if (mode == 3 && round == 9) data.saveSucceeds = false;
            ++hostMillis;
            game.loop_game();
        }
        assert(!game.capturesInstructionInput());
        if (mode == 1 || mode == 2 || mode == 6 || mode == 7)
            assert(pet.customStat(0) == 9 && data.saves == savesBefore);
        else
        {
            const uint32_t expected = mode == 5 ? 0 : 5;
            assert(pet.customStat(0) == expected);
            assert(data.saves == savesBefore + 1);
            if (mode == 3) assert(data.resourceShows > 0 && !strcmp(data.lastShownResource, "pet state"));
            if (mode == 4) data.failedAnimationId = 5;
            const int attempts = data.saves;
            for (int repeat = 0; repeat < 3; ++repeat) game.loop_game();
            assert(pet.customStat(0) == expected && data.saves == attempts);
#if ENABLE_DEATH
            if (mode == 5) assert(pet.isDead() && data.evolutionLookups == 0 && data.displayedAnimation.animationId == 9);
#endif
        }
    }
    puts("[PASS] Deferred ordered clamps: 9,+5,-5 ->5; questions/feedback unchanged; cancel/read failure discard; one save; failed save and visual failure never replay; death before evolution");
}

void testInstructionBatteryReturn()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = data.commandInstruction = data.navigationButtons = data.instructionEffects = true;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    assert(game.setup_game());
    game.synchronizeTime(0);
    const auto before = pet.statSnapshot();
    game.OnConfirmKey();
    game.instructionInput(0, 0, 0);
    game.loop_game();
    assert(game.capturesInstructionInput() && game.preventsIdleSleep());
    game.synchronizeTime(8);
    game.startBatteryAnimation();
    assert(!game.capturesInstructionInput());
    game.endBatteryAnimation();
    game.loop_game();
    game.synchronizeTime(8);
    assert(pet.customStat(0) == before.customStats[0] && pet.stageDays() == before.stage_days);
    game.OnConfirmKey();
    assert(game.capturesInstructionInput());
    game.instructionInput(0, 0, 0);
    game.loop_game();
    game.instructionInput(5, 5, 1);
    hostMillis = 1001;
    game.loop_game();
    assert(!game.capturesInstructionInput() && pet.customStat(0) == before.customStats[0]);
    game.synchronizeTime(10);
    assert(pet.stageDays() == before.stage_days + 1);
    puts("[PASS] battery interruption discards the session, excludes game RTC time, permits a fresh game and normal clock return");
}

void testInstructionFeedbackInterruption()
{
    for (bool failedFrame : {false, true})
    {
        hostMillis = 0;
        Scenario data;
        scenario = &data;
        data.commandAction = data.commandInstruction = data.navigationButtons = data.instructionEffects = true;
        data.instructionSettings = {1, 500, 10000, {1, 1, 1}};
        data.instructionAnimatedFeedback = true;
        data.instructionFeedbackFrames = 3;
        data.instructionFrameMs = 50;
        data.feedbackFirstDrawMs = 700;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        assert(game.setup_game());
        const int saves = data.saves;
        game.OnConfirmKey();
        game.instructionInput(0, 0, 0);
        game.loop_game();
        hostMillis = 500;
        game.loop_game();
        assert(hostMillis == 1200 && data.instructionFeedbackDraws == 1);
        hostMillis = 1250;
        game.loop_game();
        if (failedFrame) data.failedAnimationId = 95;
        hostMillis = 1300;
        game.loop_game();
        if (!failedFrame)
        {
            game.instructionPending(true, 1349, 5);
            hostMillis = 1350;
            game.loop_game();
            game.loop_game();
            assert(game.capturesInstructionInput() && data.instructionFeedbackFrame == 3);
            game.instructionInput(5, 5, 1349);
            game.instructionPending(false, 1369, 5);
            hostMillis = 2348;
            game.loop_game();
            assert(game.capturesInstructionInput());
            ++hostMillis;
            game.loop_game();
        }
        assert(!game.capturesInstructionInput() && pet.customStat(0) == 9 && data.saves == saves);
        assert((data.resourceShows != 0) == failedFrame);
    }
    puts("[PASS] final feedback frame failure or pending exit cancels before settlement even after animation completion");
}

void testInstructionFeedbackDrawLatency()
{
    hostMillis = 0;
    Scenario data;
    scenario = &data;
    data.commandAction = data.commandInstruction = data.navigationButtons = data.instructionEffects = true;
    data.instructionSettings = {1, 500, 10000, {1, 1, 1}};
    data.instructionAnimatedFeedback = true;
    data.instructionFeedbackFrames = 3;
    data.instructionFrameMs = 50;
    data.feedbackFirstDrawMs = data.feedbackFrameDrawMs = 700;
    SdFat sd;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(nullptr, resources);
    FakeAppearanceLoader appearance;
    Game game(pet, storage, renderer, resources, appearance);
    assert(game.setup_game());
    const int saves = data.saves;
    game.OnConfirmKey();
    game.instructionInput(0, 0, 0);
    game.loop_game();
    hostMillis = 500;
    game.loop_game();
    assert(hostMillis == 1200);
    for (uint8_t frame = 1; frame < 3; ++frame)
    {
        game.loop_game();
        assert(data.instructionFeedbackFrame == frame);
        hostMillis += 49;
        game.loop_game();
        assert(data.instructionFeedbackFrame == frame && data.saves == saves);
        ++hostMillis;
        const unsigned long beforeDraw = hostMillis;
        game.loop_game();
        assert(hostMillis == beforeDraw + 700 && data.instructionFeedbackFrame == frame + 1);
    }
    hostMillis += 49;
    game.loop_game();
    assert(game.capturesInstructionInput() && data.saves == saves);
    ++hostMillis;
    game.loop_game();
    game.loop_game();
    assert(!game.capturesInstructionInput() && data.saves == saves + 1 && pet.customStat(0) == 4);
    puts("[PASS] slow SD/display draws do not consume the next feedback frame's interval or final hold");
}

void testInstructionLifecycle()
{
    const InstructionResponseSettings configurations[] = {
        {}, {1, 500, 100, {65535, 0, 0}}, {20, 10000, 10000, {0, 65535, 65535}},
        {1, 500, 100, {1, 1, 1}, true, false, {1, 0}},
        {20, 500, 100, {65535, 65535, 65535}, true, true, {0, 65535}}
    };
    for (const auto &settings : configurations)
    for (uint16_t frameMs : {50, 10000})
    for (bool cancel : {false, true})
    for (bool customArea : {false, true})
    for (bool animatedFeedback : {false, true})
    for (uint8_t feedbackFrames : {1, 3})
    {
        hostMillis = 0;
        Scenario data;
        data.instructionSettings = settings;
        data.instructionFrameMs = frameMs;
        data.instructionCustomArea = customArea;
        data.instructionAnimatedFeedback = animatedFeedback;
        data.instructionFeedbackFrames = feedbackFrames;
        data.feedbackFirstDrawMs = 700;
        scenario = &data;
        data.commandAction = data.commandInstruction = data.navigationButtons = true;
        SdFat sd;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(nullptr, resources);
        FakeAppearanceLoader appearance;
        Game game(pet, storage, renderer, resources, appearance);
        assert(game.setup_game());
        game.synchronizeTime(0);
        const auto before = pet.statSnapshot();
        game.OnConfirmKey();
        assert(game.capturesInstructionInput() && game.preventsIdleSleep());
        game.instructionInput(0, 0, 0);
        game.loop_game();
        if (cancel)
        {
            hostMillis = settings.answerMs - 100;
            game.instructionInput(5, 5, hostMillis);
            game.loop_game();
            hostMillis = settings.answerMs;
            rtcLoop(game);
            assert(game.capturesInstructionInput());
            hostMillis = settings.answerMs + 900;
            rtcLoop(game);
        }
        else
        {
            for (unsigned round = 0; round < settings.roundCount; ++round)
            {
                game.instructionInput(0, 0, hostMillis);
                game.loop_game();
                hostMillis += settings.answerMs - 1;
                rtcLoop(game);
                assert(data.instructionFeedbackDraws == round);
                hostMillis += 1;
                rtcLoop(game);
                if (data.instructionFeedbackDraws != round + 1)
                    fprintf(stderr, "instruction mismatch: hard=%d noGo=%d rounds=%u round=%u frameMs=%u feedback=%d active=%d draws=%u expected=%u now=%lu animation=%u\n",
                        settings.hard, settings.noGoEnabled, settings.roundCount, round, frameMs, animatedFeedback,
                        game.capturesInstructionInput(), data.instructionFeedbackDraws, round + 1, hostMillis, data.displayedAnimation.animationId);
                assert(data.instructionFeedbackDraws == round + 1);
                const unsigned groups = data.instructionOptionGroups;
                if (animatedFeedback)
                {
                    for (uint8_t frame = 0; frame < feedbackFrames; ++frame)
                    {
                        hostMillis += frameMs - 1;
                        rtcLoop(game);
                        assert(game.capturesInstructionInput() && data.instructionOptionGroups == groups);
                        const int draws = data.playbackDraws;
                        ++hostMillis;
                        rtcLoop(game);
                        assert(data.playbackDraws == draws + (frame + 1 < feedbackFrames ? 1 : 0));
                        assert(game.capturesInstructionInput() && data.instructionOptionGroups == groups);
                    }
                    // Completion is consumed after the renderer confirms the final frame's hold.
                    rtcLoop(game);
                }
                else
                {
                    hostMillis += 499;
                    rtcLoop(game);
                    assert(game.capturesInstructionInput() && data.instructionOptionGroups == groups);
                    ++hostMillis;
                    rtcLoop(game);
                }
            }
        }
        assert(!game.capturesInstructionInput() && !game.preventsIdleSleep());
        const uint8_t custom[] = {16, 32, 96, 96}, none[] = {0, 0, 0, 0};
        assert(memcmp(data.animationArea, customArea ? custom : none, 4) == 0);
        assert(data.resourceShows == 0);
        assert(pet.stageDays() == before.stage_days && pet.customStat(0) == before.customStats[0]);
        const uint32_t returnedAt = hostMillis / 1000;
        game.synchronizeTime(returnedAt);
        assert(pet.stageDays() == before.stage_days && !game.hasPendingTime());
        game.synchronizeTime(returnedAt + 2);
        assert(pet.stageDays() == before.stage_days + 1);
    }
    puts("[PASS] Game 1/10/20 rounds: delayed builtin feedback holds 500ms; authored 1/3-frame feedback plays once including final hold regardless of feedback_ms; completion/cancel restores geometry without PetClock catch-up");
}
#endif

int main(int argc, char **argv)
{
#if ENABLE_INSTRUCTION_RESPONSE
    if (argc == 2 && strcmp(argv[1], "--instruction") == 0)
    {
        testInstructionResetOwnership();
        testInstructionLifecycle();
        testInstructionBatteryReturn();
        testInstructionFeedbackInterruption();
        testInstructionFeedbackDrawLatency();
        testInstructionDeferredEffects();
        return 0;
    }
#endif
#if ENABLE_COMMAND_PREDICT
    if (argc == 2 && strcmp(argv[1], "--predict") == 0)
    {
        testPredictMinigame();
#if ENABLE_GUESS_GAME
        testSwitchFromGuessToPredict();
#endif
        return 0;
    }
#endif
#if ENABLE_SEQUENCE_AUDIO
    if (argc == 2 && strcmp(argv[1], "--idle-audio") == 0)
    {
        testIdleMusicGameLifecycle();
        puts("[PASS] Game idle music leaves pet clock/RTC catch-up active; Action return, STOP redraw, death and chooser interruption");
        return 0;
    }
#endif
    if (argc == 2 && strcmp(argv[1], "--death") == 0)
    {
        testDeathCompileFlag();
        testSavedDeathReset();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--evolution") == 0)
    {
        testEvolutionResults();
        testExportedEvolutionPlaybackCounts();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--status") == 0)
    {
        testStatusReadsActionCommittedPet();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--sequence-interruption") == 0)
    {
        testActionPlaybackPausesPetTime();
        testBatchedCatchUpMatchesDailyEvents();
        testCommandReplacesSequenceAcrossDayBoundary();
        testRtcCatchUp();
        testRtcSnapshotAndReset();
        testNavigationDuringRtcCatchUp();
        return 0;
    }
#if !ENABLE_DAILY_POST_PROCESSING
    testDailyPostProcessingDisabled();
    testNavigationDuringRtcCatchUp();
    testGameRoutesUserActionResult();
    return 0;
#endif
    testFourSecondPeriodicSave();
    testActionPlaybackPausesPetTime();
    testBatchedCatchUpMatchesDailyEvents();
    testDailySaveRetry();
    testSaveRequestResults();
    testNavigationDuringRtcCatchUp();
    testRtcCatchUp();
    testRtcSnapshotAndReset();
#if ENABLE_COMMAND_OUTFIT
    testForegroundInputOwnership();
#endif
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
    testUserActionPlaybackSequence();
    testCommandReplacesSequenceAcrossDayBoundary();
    testGameRoutesUserActionResult();
    testUnifiedCommandExecution();
    testStatusReadsActionCommittedPet();
#if ENABLE_COMMAND_PREDICT
    testPredictMinigame();
#if ENABLE_GUESS_GAME
    testSwitchFromGuessToPredict();
#endif
#endif
}
