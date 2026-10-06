#ifndef EVOLUTION_CONTROLLER_H
#define EVOLUTION_CONTROLLER_H

#include "animation/Animation.h"
#include "appearance/AppearanceLoader.h"
#include "appearance/AppearanceChangeController.h"

class AnimationController;
class Pet;
class Renderer;

// Existing appearance operations; callers never receive Evolution stages.
class EvolutionHost
{
public:
    virtual AppearanceChangeResult enterSpecies(uint8_t speciesSlot, uint8_t outfitSlot) = 0;
    virtual void refreshBaseAnimation() = 0;

protected:
    ~EvolutionHost() = default;
};

// NoChange permits normal ticking; every other result consumes this turn.
// Failed requests a resource message, FatalFailure additionally stops the game.
enum class EvolutionResult : uint8_t
{
    NoChange,
    InProgress,
    Completed,
    Failed,
    FatalFailure,
};

class EvolutionController
{
public:
    EvolutionController(Pet &pet, AnimationController &animations,
                        AppearanceLoader &appearanceLoader, Renderer &renderer,
                        EvolutionHost &host);

    EvolutionResult check();
    EvolutionResult update(PlaybackResult result);
    // Forget an interrupted Evolution. Playback cancellation belongs to its owner.
    void cancel();
    bool isActive() const;

private:
    enum class Phase : uint8_t
    {
        None,
        SourceSegment,
        ApplyingTarget,
        TargetSegment,
    };

    Pet &pet;
    AnimationController &animations;
    AppearanceLoader &appearanceLoader;
    Renderer &renderer;
    EvolutionHost &host;
    Phase phase = Phase::None;
    uint8_t targetSpeciesSlot = 0;
    uint8_t targetOutfitSlot = 0;
    AssetData::AnimationRef targetAnimation = {};
    uint8_t targetPlaybackCount = 0;

    bool begin(const AppearanceSelection &selection);
    EvolutionResult advance();
    void finish();
};

#endif // EVOLUTION_CONTROLLER_H
