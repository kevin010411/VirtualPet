#include "appearance/EvolutionController.h"

#include "animation/AnimationController.h"
#include "pet/Pet.h"
#include "resources/AssetResources.h"

namespace
{
EvolutionResult playbackFailure(const AssetResources &resources)
{
    return resources.firstAssetDataError() == AssetData::BundleError::None
        ? EvolutionResult::Failed : EvolutionResult::FatalFailure;
}
}

EvolutionController::EvolutionController(Pet &petRef,
                                         AnimationController &animationsRef,
                                         AppearanceLoader &appearanceLoaderRef,
                                         AssetResources &resourcesRef, EvolutionHost &hostRef)
    : pet(petRef), animations(animationsRef),
      appearanceLoader(appearanceLoaderRef), resources(resourcesRef), host(hostRef)
{
}

EvolutionResult EvolutionController::check()
{
    if (isActive())
        return EvolutionResult::InProgress;

    AppearanceSelection selection = {};
    const EvolutionLookupResult result =
        appearanceLoader.findEvolutionTarget(pet.statSnapshot(), selection);
    if (result == EvolutionLookupResult::LoadFailed)
    {
        resources.recordAssetDataErrorResource(appearanceLoader.firstAssetDataErrorResource());
        return EvolutionResult::FatalFailure;
    }
    if (result != EvolutionLookupResult::Found || selection.speciesSlot == pet.speciesSlot())
        return EvolutionResult::NoChange;

    if (selection.evolutionMode == EvolutionAnimationMode::Disabled)
    {
        const AppearanceChangeResult applied = host.enterSpecies(selection.speciesSlot, selection.outfitSlot);
        if (applied != AppearanceChangeResult::Applied)
            return applied == AppearanceChangeResult::ConfigurationFailed
                ? EvolutionResult::FatalFailure : EvolutionResult::Failed;
        finish();
        return EvolutionResult::Completed;
    }

    return begin(selection) ? EvolutionResult::InProgress : playbackFailure(resources);
}

bool EvolutionController::begin(const AppearanceSelection &selection)
{
    if (!animations.hasAnimation(selection.sourceEvolutionAnimation))
        return false;

    targetSpeciesSlot = selection.speciesSlot;
    targetOutfitSlot = selection.outfitSlot;
    targetAnimation = selection.targetEvolutionAnimation;
    targetPlaybackCount = selection.targetEvolutionPlaybackCount;
    phase = Phase::SourceSegment;
    const Animation animation = Animation::complete(
        selection.sourceEvolutionAnimation, selection.sourceEvolutionPlaybackCount,
        FirmwarePlaybackRole::Evolution);
    if (animations.replace(AnimationSequence(&animation, 1)) != PlaybackResult::Accepted)
    {
        cancel();
        return false;
    }
    return true;
}

EvolutionResult EvolutionController::update(PlaybackResult result)
{
    if (!isActive())
        return EvolutionResult::NoChange;
    if (result == PlaybackResult::PlaybackFailed)
    {
        animations.cancelAll();
        if (resources.firstAssetDataError() != AssetData::BundleError::None)
        {
            cancel();
            return EvolutionResult::FatalFailure;
        }
        if (phase == Phase::SourceSegment || phase == Phase::ApplyingTarget)
        {
            cancel();
            return EvolutionResult::Failed;
        }
        else
            finish();
        // The target appearance is already committed. Preserve the existing
        // silent recovery to its base animation on a non-resource frame failure.
        return EvolutionResult::Completed;
    }
    return result == PlaybackResult::Accepted ? advance() : EvolutionResult::InProgress;
}

EvolutionResult EvolutionController::advance()
{
    if (animations.isBusy())
        return EvolutionResult::InProgress;

    if (phase == Phase::SourceSegment)
        phase = Phase::ApplyingTarget;

    if (phase == Phase::ApplyingTarget)
    {
        const AppearanceChangeResult applied = host.enterSpecies(targetSpeciesSlot, targetOutfitSlot);
        if (applied != AppearanceChangeResult::Applied)
        {
            cancel();
            return applied == AppearanceChangeResult::ConfigurationFailed
                ? EvolutionResult::FatalFailure : EvolutionResult::Failed;
        }
    }

    if (phase == Phase::ApplyingTarget && targetAnimation.valid())
    {
        const Animation animation = Animation::complete(
            targetAnimation, targetPlaybackCount, FirmwarePlaybackRole::Evolution);
        if (animations.replace(AnimationSequence(&animation, 1)) == PlaybackResult::Accepted)
        {
            phase = Phase::TargetSegment;
            return EvolutionResult::InProgress;
        }
        finish();
        return playbackFailure(resources);
    }
    finish();
    return EvolutionResult::Completed;
}

void EvolutionController::finish()
{
    // Species entry has already committed the appearance and its one save.
    // Both completion and target playback failure return to the current base.
    host.refreshBaseAnimation();
    // Playback compares the new layout ID and values; unchanged buttons stay.
    cancel();
}

void EvolutionController::cancel()
{
    phase = Phase::None;
    targetSpeciesSlot = 0;
    targetOutfitSlot = 0;
    targetAnimation = {};
    targetPlaybackCount = 0;
}

bool EvolutionController::isActive() const
{
    return phase != Phase::None;
}
