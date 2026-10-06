#include "appearance/EvolutionController.h"

#include "animation/AnimationController.h"
#include "pet/Pet.h"
#include "display/Renderer.h"

namespace
{
EvolutionResult playbackFailure(const Renderer &renderer)
{
    return renderer.firstAssetDataError() == AssetData::BundleError::None
        ? EvolutionResult::Failed : EvolutionResult::FatalFailure;
}
}

EvolutionController::EvolutionController(Pet &petRef,
                                         AnimationController &animationsRef,
                                         AppearanceLoader &appearanceLoaderRef,
                                         Renderer &rendererRef, EvolutionHost &hostRef)
    : pet(petRef), animations(animationsRef),
      appearanceLoader(appearanceLoaderRef), renderer(rendererRef), host(hostRef)
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
        renderer.recordAssetDataErrorResource(appearanceLoader.firstAssetDataErrorResource());
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

    return begin(selection) ? EvolutionResult::InProgress : playbackFailure(renderer);
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
        if (renderer.firstAssetDataError() != AssetData::BundleError::None)
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
        return playbackFailure(renderer);
    }
    finish();
    return EvolutionResult::Completed;
}

void EvolutionController::finish()
{
    // Species entry has already committed the appearance and its one save.
    // Both completion and target playback failure return to the current base.
    host.refreshBaseAnimation();
    animations.requestFullRedraw();
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
