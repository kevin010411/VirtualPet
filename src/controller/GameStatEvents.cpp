#include "controller/Game.h"

#if ENABLE_STAT_EVENTS
#include "animation/AnimationController.h"
#include "display/Renderer.h"
#include "collection/CollectionGameEvent.h"
#include "pet/Pet.h"
#include "resources/AssetResources.h"

namespace
{
bool wouldDie(const PetBehaviorConfig &config, const PetStatSnapshot &values)
{
#if ENABLE_DEATH
    if (config.deathEnabled)
        for (uint8_t slot = 0; slot < kPetCustomStatCount; ++slot)
            if ((config.deathStatMask & (1U << slot)) && values.customStats[slot] == 0) return true;
#else
    (void)config;
    (void)values;
#endif
    return false;
}
}

bool Game::statEventsActive() const
{
    return petBehaviorConfig.statEventTriggerCount != 0;
}

bool Game::isRoundComplete() const
{
    return statEventsActive() && pet.isRoundComplete();
}

bool Game::settleStatMutation(const PetStatSnapshot &after, uint8_t actionSlot, bool daily)
{
    const PersistedPetState previous = pet.persistentState();
    const PetStatSnapshot before = pet.statSnapshot();
    if (pet.isRoundComplete()) return false;
    PetStatEventResult events = {};
    // User Actions already give their immediate death condition precedence in Web.
    if (actionSlot != UINT8_MAX && wouldDie(petBehaviorConfig, after))
        events.values = after;
    else if (!settleStatEvents(resources.sdCard(), petBehaviorConfig, before, after, events))
    {
        resources.recordAssetDataErrorResource("runtime.bin");
        return false;
    }
    if (events.pageCount > sizeof(events.pages) / sizeof(events.pages[0]) - statEventPageCount)
        return false;

    if (!pet.commitPetStats(events.values.customStats, kPetCustomStatCount)) return false;
    pet.setStageDays(events.values.stage_days);
    if (petBehaviorConfig.persistentStatEvents) pet.setActiveStatEvent(events.values.activeStatEvent);
#if ENABLE_COLLECTION_CATALOG
    for (uint8_t index = 0; index < petBehaviorConfig.trackedActionCount; ++index)
        if (petBehaviorConfig.trackedActions[index] == actionSlot)
        {
            pet.incrementActionCount(index);
            break;
        }
#else
    (void)actionSlot;
#endif
    if (events.terminal && !pet.isDead()) pet.completeRound();

    AppearanceSelection target = {};
    bool ready = true;
    bool activatedTarget = false;
    if (!pet.isDead() && !pet.isRoundComplete() && daily)
    {
        const auto evolutionResult = appearanceLoader.findEvolutionTarget(pet.statSnapshot(), target);
        if (evolutionResult == EvolutionLookupResult::LoadFailed)
        {
            resources.recordAssetDataErrorResource(appearanceLoader.firstAssetDataErrorResource());
            ready = false;
        }
        else if (evolutionResult == EvolutionLookupResult::Found && target.speciesSlot != before.speciesSlot)
        {
            activatedTarget = true;
            ready = configureActiveAppearance(target.speciesSlot, target.outfitSlot) &&
                pet.setSpeciesSlot(target.speciesSlot) && pet.setOutfitSlot(target.outfitSlot);
#if ENABLE_SHOP
            bool changed = false;
            if (ready) ready = shop.arrive(changed);
#endif
        }
        else target = {};
    }
    if (ready && !pet.isDead()) ready = refreshOutfitUnlockMask(activatedTarget, false);
#if ENABLE_COLLECTION_GAME_EVENTS
    if (ready) ready = recordCollectionGameEvent(resources.sdCard(), petBehaviorConfig, pet, previous,
                                                 CollectionGameEvent::StatTransition);
#endif
    if (ready) ready = petSaves.saveCollectionProgress(true, true) == PetSaveResult::Persisted;
    if (!ready)
    {
        pet.restoreState(previous);
        if (activatedTarget && !configureActiveAppearance(before.speciesSlot, before.outfitSlot))
            enterRuntimeFatal();
        return false;
    }

#if ENABLE_COLLECTION_CATALOG
    collectionSavePending = false;
#endif
    if (!pet.isDead())
    {
#if ENABLE_GUESS_GAME
        if (events.terminal && flow.is(AppStage::Minigame))
            minigame->reset();
#endif
        for (uint8_t index = 0; index < events.pageCount; ++index)
            statEventPages[statEventPageCount++] = events.pages[index];
        committedEvolution = target;
        refreshBaseAnimation();
    }
    return true;
}

bool Game::redrawStatEvents()
{
    if (statEventAnimationId)
    {
        if (!renderer.ShowAnimationFrame({0, 0, statEventAnimationId}, 0, 1, 0, 0, 12, 128, 160))
            enterRuntimeFatal();
        return true;
    }
    if (!isRoundComplete() || statEventPageCount) return false;
    renderer.showRoundComplete();
    dirtySelect = false;
    return true;
}

bool Game::presentStatEvents(unsigned long now)
{
    const bool presenting = statEventPageCount || committedEvolution.speciesSlot || isRoundComplete();
    if (presenting)
    {
        if (animations->hasFinitePlayback())
        {
            handlePlaybackResult(tickPlayback(now).result);
            return true;
        }
    }
    if (statEventAnimationId)
    {
        if (now - statEventPageStarted < statEventPageDuration) return true;
        statEventAnimationId = 0;
        ++statEventPage;
    }
    if (statEventPage < statEventPageCount)
    {
        if (!readStatEventPage(resources.sdCard(), petBehaviorConfig, statEventPages[statEventPage],
                               statEventAnimationId, statEventPageDuration))
        {
            resources.recordAssetDataErrorResource("runtime.bin");
            enterRuntimeFatal();
        }
        else
        {
            statEventPageStarted = now;
            redrawStatEvents();
        }
        return true;
    }

    if (statEventPageCount)
    {
        statEventPageCount = statEventPage = 0;
        animations->requestFullRedraw();
        dirtySelect = true;
    }
    if (isRoundComplete())
    {
        if (dirtySelect)
        {
            renderer.showRoundComplete();
            dirtySelect = false;
        }
        return true;
    }
    if (committedEvolution.speciesSlot)
    {
        const auto target = committedEvolution;
        committedEvolution = {};
        Animation sequence[2];
        uint8_t count = 0;
        if (target.sourceEvolutionAnimation.valid())
            sequence[count++] = Animation::complete(target.sourceEvolutionAnimation, target.sourceEvolutionPlaybackCount);
        if (target.targetEvolutionAnimation.valid())
            sequence[count++] = Animation::complete(target.targetEvolutionAnimation, target.targetEvolutionPlaybackCount);
        if (count)
        {
            if (animations->replace(AnimationSequence(sequence, count)) == PlaybackResult::Accepted)
            {
                playingCommittedEvolution = true;
                flow.beginEvolution();
            }
            else renderer.showResourceError();
        }
        animations->requestFullRedraw();
    }
    if (presenting) synchronizePlaybackClock();
    return false;
}

void Game::clearStatEventPresentation()
{
    statEventPageCount = statEventPage = 0;
    statEventAnimationId = 0;
    committedEvolution = {};
    playingCommittedEvolution = false;
}
#endif
