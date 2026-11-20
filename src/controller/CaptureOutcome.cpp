#include "common/AppProfile.h"

#if ENABLE_CAPTURE_GAME
#include "controller/Game.h"
#include "collection/CollectionGameEvent.h"
#include "pet/Pet.h"
#include "resources/AssetResources.h"
#if ENABLE_COLLECTION_GAME_EVENTS
#include <string.h>
#endif

namespace
{
void applyDelta(const PetBehaviorConfig &config, PetStatSnapshot &values, uint8_t slot, int32_t delta)
{
    if (slot == CaptureAssets::stageDaysSlot)
    {
        const int64_t next = static_cast<int64_t>(values.stage_days) + delta;
        values.stage_days = next < 0 ? 0 : next > Pet::kStageDaysMax ? Pet::kStageDaysMax : next;
        return;
    }

    const auto &stat = config.stats[slot];
    const int64_t next = static_cast<int64_t>(values.customStats[slot]) + delta;
    values.customStats[slot] = next < stat.minValue ? stat.minValue :
        next > stat.maxValue ? stat.maxValue : static_cast<uint32_t>(next);
}
}

bool Game::onCaptureOutcome(CaptureAssets &assets, const CaptureAssets::Target *target,
                            const CollectionRoundStats &round, int32_t score,
                            uint16_t &increasedStats)
{
    const PersistedPetState before = pet.persistentState();
    PetStatSnapshot values = pet.statSnapshot();
    if (target != nullptr && target->group != CaptureAssets::noSlot)
    {
        CaptureAssets::Group group;
        if (!assets.readGroup(target->group, group)) return false;
        const auto catches = round.groupCatches[target->group];
        for (uint8_t index = 0; index < group.effectCount; ++index)
        {
            CaptureAssets::Effect effect;
            if (!assets.readEffect(group.firstEffect + index, effect)) return false;
            if ((effect.mode == 1 && catches > effect.limit) ||
                (effect.mode == 2 && catches <= effect.limit)) continue;
            applyDelta(petBehaviorConfig, values, effect.stat, effect.delta);
        }
    }
    if (target == nullptr)
    {
        const auto &reward = assets.scene().reward;
        if (reward.stat != CaptureAssets::noSlot)
        {
            const uint32_t points = score > 0 ? score : 0;
            const uint64_t earned = reward.participation +
                static_cast<uint64_t>(points / reward.perPoints) * reward.coins;
            const uint16_t amount = earned > reward.cap ? reward.cap : earned;
            applyDelta(petBehaviorConfig, values, reward.stat, amount);
        }
    }

    uint16_t gained = 0;
    bool changed = before.stage_days != values.stage_days;
    if (values.stage_days > before.stage_days) gained |= 1U << CaptureAssets::stageDaysSlot;
    for (uint8_t slot = 0; slot < petBehaviorConfig.statCount; ++slot)
    {
        changed |= values.customStats[slot] != before.customStats[slot];
        if (values.customStats[slot] > before.customStats[slot]) gained |= 1U << slot;
    }
    if (changed)
    {
        if (petBehaviorConfig.statCount &&
            !pet.commitPetStats(values.customStats, petBehaviorConfig.statCount)) return false;
        pet.setStageDays(values.stage_days);
    }
#if ENABLE_COLLECTION_CATALOG
    if (petBehaviorConfig.collectionFingerprint != 0)
    {
        if (target != nullptr && target->collection != CaptureAssets::noSlot &&
            !pet.isCollectionOwned(target->collection))
        {
            pet.acquireCollection(target->collection);
            changed = true;
        }
#if ENABLE_COLLECTION_GAME_EVENTS
        const auto event = target == nullptr ? CollectionGameEvent::CaptureRound : CollectionGameEvent::CaptureCatch;
        if (!recordCollectionGameEvent(resources.sdCard(), petBehaviorConfig, pet, before, event,
                                       CaptureAssets::noSlot, &round))
        {
            pet.restoreState(before);
            return false;
        }
        const auto &after = pet.persistentState();
        changed |= before.collectionProgressFingerprint != after.collectionProgressFingerprint ||
            memcmp(before.collectionProgress, after.collectionProgress, sizeof(before.collectionProgress)) != 0;
#endif
    }
#endif
    if (changed && petSaves.saveCollectionProgress(true, true) != PetSaveResult::Persisted)
    {
        pet.restoreState(before);
        return false;
    }
    increasedStats = gained;
    return true;
}
#endif
