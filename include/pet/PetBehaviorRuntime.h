#ifndef PET_BEHAVIOR_RUNTIME_H
#define PET_BEHAVIOR_RUNTIME_H

#include <stdint.h>

#include "animation/Animation.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "pet/PetStateClassifier.h"

class AnimationController;
class Pet;
class AssetResources;
class AppearanceLoader;
struct PetStatSnapshot;
struct PersistedPetState;

enum class PetBehaviorActionResult : uint8_t
{
    Rejected,
    ResourceReadFailed,
    Applied,
    AppliedAnimationMissing,
    AwaitingCompletion,
};

enum class PendingActionProgress : uint8_t { Playing, Evolve, Settled, Failed };

class PetBehaviorRuntime
{
public:
    PetBehaviorRuntime(const PetBehaviorConfig &config,
                       Pet &pet,
                       AnimationController &animations,
                       AssetResources &resources);

    bool hasAction(uint8_t actionSlot) const;
    void setPersistence(bool (*save)(void *), void *context) { saveMutation = save; saveContext = context; }
#if ENABLE_STAT_EVENTS
    void setStatEventSettlement(bool (*settle)(void *, const PetStatSnapshot &, uint8_t, bool))
    {
        settleStatMutation = settle;
    }
#endif
    void initializeStats();
    void resetTransientState() { dailyChangePauses = {}; pendingPlaybackStarted = false; }
    void interruptPendingActionPlayback() { pendingPlaybackStarted = false; }
    bool isDead() const;
    bool advancePetDay(uint32_t days = 1);
    bool catchUpDays(AppearanceLoader &loader, uint32_t limit, uint32_t &days) const;
    PetBehaviorActionResult executeAction(uint8_t actionSlot);
    PendingActionProgress continuePendingAction(uint8_t &species, uint8_t &outfit);
#if ENABLE_GUESS_GAME
    bool applyGuessOutcome(PetBehaviorGuessOutcome outcome);
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    bool applyInstructionResults(uint32_t correctMask, uint8_t count);
#endif
    AssetData::AnimationRef baseAnimation() const;
    ActivePetState activePetState() const;
    ActivePetState activePetState(const PetStatSnapshot &snapshot) const;

private:
    const PetBehaviorConfig &config;
    Pet &pet;
    AnimationController &animations;
    AssetResources &resources;
    PetBehaviorDailyChangePauses dailyChangePauses;
    bool pendingPlaybackStarted = false;
    PlaybackResult playAction(const PetBehaviorActionConfig &action, const PetBehaviorActionPlayback &playback);
    PetBehaviorActionResult commitActionValues(const PetBehaviorStatValues &state, uint8_t slot, const PersistedPetState &before);
    bool (*saveMutation)(void *) = nullptr;
    void *saveContext = nullptr;
#if ENABLE_STAT_EVENTS
    bool (*settleStatMutation)(void *, const PetStatSnapshot &, uint8_t, bool) = nullptr;
    bool usesStatEvents() const { return config.statEventTriggerCount && settleStatMutation; }
    bool commitStatMutation(const PetBehaviorStatValues &state, uint8_t actionSlot, uint32_t days = 0);
#endif
};

#endif // PET_BEHAVIOR_RUNTIME_H
