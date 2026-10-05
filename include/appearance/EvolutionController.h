#ifndef EVOLUTION_CONTROLLER_H
#define EVOLUTION_CONTROLLER_H

#include "animation/Animation.h"
#include "appearance/AppearanceLoader.h"

class AnimationController;
class Pet;
class Renderer;

// Existing appearance operations; callers never receive Evolution stages.
class EvolutionHost
{
public:
    virtual bool enterSpecies(uint8_t speciesSlot, uint8_t outfitSlot) = 0;
    virtual void refreshBaseAnimation() = 0;

protected:
    ~EvolutionHost() = default;
};

class EvolutionController
{
public:
    EvolutionController(Pet &pet, AnimationController &animations,
                        AppearanceLoader &appearanceLoader, Renderer &renderer,
                        EvolutionHost &host);

    // False means a required contract lookup failed; the game must enter fatal.
    bool check();
    // Returns whether Evolution advanced or consumed a playback failure.
    bool update(PlaybackResult result);
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

    bool begin(const AppearanceSelection &selection);
    bool advance();
    void finish();
};

#endif // EVOLUTION_CONTROLLER_H
