#include "appearance/EvolutionController.h"

#include "animation/AnimationController.h"
#include "pet/Pet.h"
#include "display/Renderer.h"

namespace
{
constexpr uint8_t kEvolutionPlaybackCount = 2;
}

EvolutionController::EvolutionController(Pet &petRef,
                                         AnimationController &animationsRef,
                                         AppearanceLoader &appearanceLoaderRef,
                                         Renderer &rendererRef, EvolutionHost &hostRef)
    : pet(petRef), animations(animationsRef),
      appearanceLoader(appearanceLoaderRef), renderer(rendererRef), host(hostRef)
{
}

bool EvolutionController::check()
{
    if (isActive())
        return true;

    AppearanceSelection selection = {};
    const EvolutionLookupResult result =
        appearanceLoader.findEvolutionTarget(pet.statSnapshot(), selection);
    if (result == EvolutionLookupResult::LoadFailed)
    {
        renderer.recordAssetDataErrorResource(appearanceLoader.firstAssetDataErrorResource());
        renderer.showResourceError();
        return false;
    }
    if (result != EvolutionLookupResult::Found || selection.speciesSlot == pet.speciesSlot())
        return true;

    if (selection.evolutionMode == EvolutionAnimationMode::Disabled)
    {
        if (host.enterSpecies(selection.speciesSlot, selection.outfitSlot))
            finish();
        else
            renderer.showResourceError();
        return true;
    }

    if (!begin(selection))
        renderer.showResourceError();
    return true;
}

bool EvolutionController::begin(const AppearanceSelection &selection)
{
    if (!animations.hasAnimation(selection.sourceEvolutionAnimation))
        return false;

    targetSpeciesSlot = selection.speciesSlot;
    targetOutfitSlot = selection.outfitSlot;
    targetAnimation = selection.targetEvolutionAnimation;
    phase = Phase::SourceSegment;
    const Animation animation = Animation::complete(
        selection.sourceEvolutionAnimation, kEvolutionPlaybackCount,
        FirmwarePlaybackRole::Evolution);
    if (animations.replace(AnimationSequence(&animation, 1)) != PlaybackResult::Accepted)
    {
        cancel();
        return false;
    }
    return true;
}

bool EvolutionController::update(PlaybackResult result)
{
    if (!isActive())
        return false;
    if (result == PlaybackResult::PlaybackFailed)
    {
        animations.cancelAll();
        if (phase == Phase::SourceSegment || phase == Phase::ApplyingTarget)
        {
            cancel();
            renderer.showResourceError();
        }
        else
            finish();
        return true;
    }
    return result == PlaybackResult::Accepted && advance();
}

bool EvolutionController::advance()
{
    if (animations.isBusy())
        return false;

    if (phase == Phase::SourceSegment)
        phase = Phase::ApplyingTarget;

    if (phase == Phase::ApplyingTarget &&
        !host.enterSpecies(targetSpeciesSlot, targetOutfitSlot))
    {
        renderer.showResourceError();
        cancel();
        return false;
    }

    if (phase == Phase::ApplyingTarget && targetAnimation.valid())
    {
        const Animation animation = Animation::complete(
            targetAnimation, kEvolutionPlaybackCount, FirmwarePlaybackRole::Evolution);
        if (animations.replace(AnimationSequence(&animation, 1)) == PlaybackResult::Accepted)
        {
            phase = Phase::TargetSegment;
            return true;
        }
        renderer.showResourceError();
    }
    finish();
    return true;
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
}

bool EvolutionController::isActive() const
{
    return phase != Phase::None;
}
