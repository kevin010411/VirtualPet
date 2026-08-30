#include "pet_behavior/domain/PetBehaviorContract.h"

#include <limits.h>
#include <string.h>
#include "commands/domain/SystemCommandCatalog.h"
#include "shared/assets/AssetRuntimeContract.h"
#include "shared/sd/SdTextRecordReader.h"
#include "pet_behavior/domain/PetBehaviorActionConditionRules.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "shared/utils/CanonicalDecimal.h"

namespace
{
constexpr const char *kRuntimeContractPath = "/runtime_contract.txt";
constexpr const char *kRuntimeContractIdentity = "runtime_contract";
constexpr const char *kRuntimeContractVersion = "4";
constexpr const char *kFlowContractPath = "/flow_contract.txt";
constexpr const char *kLayoutContractPath = "/layout_contract.txt";
constexpr size_t kMaxAuxiliaryContractBytes = 16384;

bool parseUnsigned(const char *text, uint32_t maximum, uint32_t &value)
{
    return CanonicalDecimal::parseUnsigned(text, maximum, value);
}

bool parseSigned(const char *text, uint32_t positiveMaximum, uint32_t negativeMaximum, int32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;
    bool negative = false;
    const char *cursor = text;
    if (*cursor == '-')
    {
        negative = true;
        ++cursor;
    }
    if (*cursor == '\0')
        return false;
    uint32_t magnitude = 0;
    const uint32_t maximum = negative ? negativeMaximum : positiveMaximum;
    for (; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
        if (magnitude > (maximum - digit) / 10U)
            return false;
        magnitude = magnitude * 10U + digit;
    }
    value = negative ? static_cast<int32_t>(-static_cast<int64_t>(magnitude))
                     : static_cast<int32_t>(magnitude);
    return true;
}

bool parseSigned16(const char *text, int16_t &value)
{
    int32_t parsed = 0;
    if (!parseSigned(text, 32767U, 32768U, parsed))
        return false;
    value = static_cast<int16_t>(parsed);
    return true;
}

bool parseSigned32(const char *text, int32_t &value)
{
    return parseSigned(text, 2147483647UL, 2147483648UL, value);
}

bool parseHex32(const char *text, uint32_t &value)
{
    if (text == nullptr || strlen(text) != 8)
        return false;
    uint32_t parsed = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        uint8_t digit = 0;
        if (*cursor >= '0' && *cursor <= '9')
            digit = static_cast<uint8_t>(*cursor - '0');
        else if (*cursor >= 'a' && *cursor <= 'f')
            digit = static_cast<uint8_t>(*cursor - 'a' + 10);
        else
            return false;
        parsed = (parsed << 4U) | digit;
    }
    value = parsed;
    return true;
}

bool parseSlot(const char *token, const char *prefix, uint16_t capacity, uint8_t &slot)
{
    if (token == nullptr || prefix == nullptr || capacity == 0)
        return false;
    const size_t prefixLength = strlen(prefix);
    uint32_t parsed = 0;
    if (strncmp(token, prefix, prefixLength) != 0 ||
        !parseUnsigned(token + prefixLength, capacity - 1U, parsed))
        return false;
    slot = static_cast<uint8_t>(parsed);
    return true;
}

bool copyBounded(const char *source, char *destination, size_t capacity)
{
    if (source == nullptr || destination == nullptr || source[0] == '\0' || strlen(source) >= capacity)
        return false;
    strcpy(destination, source);
    return true;
}

bool parseEffectOperation(const char *token, PetBehaviorEffectOperation &operation)
{
    if (strcmp(token, "change") == 0)
        operation = PetBehaviorEffectOperation::Change;
    else if (strcmp(token, "set") == 0)
        operation = PetBehaviorEffectOperation::Set;
    else
        return false;
    return true;
}

class PetBehaviorDecoder
{
public:
    PetBehaviorDecoder(const AssetData::RuntimeManifest &manifest,
                       uint8_t speciesSlot,
                       uint8_t outfitSlot,
                       BundleReader *bundleReader = nullptr,
                       bool migrationComposition = false)
        : manifest_(manifest), speciesSlot_(speciesSlot), outfitSlot_(outfitSlot),
          bundleReader_(bundleReader), migrationComposition_(migrationComposition)
    {
        candidate.assetManifest = manifest;
        candidate.activeSpeciesSlot = speciesSlot;
        candidate.activeOutfitSlot = outfitSlot;
    }

    bool process(const SdTextRecord &record)
    {
        if (record.fieldCount == 0)
            return false;
        if (strcmp(record.fields[0], "asset_data") == 0)
            return decodeAssetData(record);
        if (strcmp(record.fields[0], "bundle_id") == 0)
            return decodeBundleId(record);
        if (!assetDataSeen || !bundleSeen)
            return false;
        if (migrationComposition_ && binaryOwnedRecord(record.fields[0]))
            return true;
        if (strcmp(record.fields[0], "pet_behavior") == 0)
            return decodePetBehaviorHeader(record);
        if (strcmp(record.fields[0], "stat") == 0)
            return decodeStat(record);
        if (strcmp(record.fields[0], "idle") == 0)
            return decodeIdle(record);
        if (strcmp(record.fields[0], "idle_trigger") == 0)
            return decodeIdleTrigger(record);
        if (strcmp(record.fields[0], "action") == 0)
            return decodeAction(record);
        if (strcmp(record.fields[0], "action_animation") == 0)
            return decodeActionAnimation(record);
        if (strcmp(record.fields[0], "action_outcome") == 0)
            return decodeActionOutcome(record);
        if (strcmp(record.fields[0], "action_condition") == 0)
            return decodeActionCondition(record);
        if (strcmp(record.fields[0], "action_effect") == 0)
            return decodeActionEffect(record);
        if (strcmp(record.fields[0], "action_outcome_effect") == 0)
            return decodeActionOutcomeEffect(record);
#if ENABLE_GUESS_GAME
        if (strcmp(record.fields[0], "guess_effect") == 0)
            return decodeGuessEffect(record);
#endif
        if (strcmp(record.fields[0], "button") == 0)
            return decodeButton(record);
        if (strcmp(record.fields[0], "status") == 0)
            return decodeStatusHeader(record);
        if (strcmp(record.fields[0], "status_set") == 0)
            return decodeStatusSet(record);
        if (strcmp(record.fields[0], "status_animation") == 0)
            return decodeStatusAnimation(record);
        if (strcmp(record.fields[0], "status_condition") == 0)
            return decodeStatusCondition(record);
        return false;
    }

    bool complete(PetBehaviorConfig &destination) const
    {
        if (!assetDataSeen || !bundleSeen || !idleSeen)
            return false;
        if (migrationComposition_)
        {
            destination = candidate;
            return true;
        }
        if (!identitySeen || !statusSeen ||
            candidate.buttonCount != kPetBehaviorButtonCount ||
            candidate.statCount > kMaxPetBehaviorStats || !validActions() || !validStatusConditions()
#if ENABLE_GUESS_GAME
            || !validGuessEffects()
#endif
        )
            return false;
        destination = candidate;
        return true;
    }

private:
    PetBehaviorConfig candidate = {};
    const AssetData::RuntimeManifest &manifest_;
    uint8_t speciesSlot_;
    uint8_t outfitSlot_;
    BundleReader *bundleReader_;
    bool migrationComposition_;
    bool assetDataSeen = false;
    bool bundleSeen = false;
    bool identitySeen = false;
    bool statusSeen = false;
    bool idleSeen = false;

    static bool binaryOwnedRecord(const char *kind)
    {
        return strcmp(kind, "pet_behavior") == 0 || strcmp(kind, "stat") == 0 ||
               strcmp(kind, "action") == 0 || strcmp(kind, "action_animation") == 0 ||
               strcmp(kind, "action_outcome") == 0 || strcmp(kind, "action_condition") == 0 ||
               strcmp(kind, "action_effect") == 0 ||
               strcmp(kind, "action_outcome_effect") == 0 || strcmp(kind, "button") == 0 ||
               strcmp(kind, "status") == 0 || strcmp(kind, "status_set") == 0 ||
               strcmp(kind, "status_animation") == 0 || strcmp(kind, "status_condition") == 0;
    }

    bool decodeAssetData(const SdTextRecord &record)
    {
        if (assetDataSeen || record.fieldCount != 2 || strcmp(record.fields[1], "1") != 0)
            return false;
        assetDataSeen = true;
        return true;
    }

    bool decodeBundleId(const SdTextRecord &record)
    {
        AssetData::BundleId bundleId = {};
        if (bundleSeen || record.fieldCount != 2 ||
            !AssetData::parseBundleId(record.fields[1], bundleId) ||
            !AssetData::sameBundleId(bundleId, manifest_.bundleId))
            return false;
        bundleSeen = true;
        return true;
    }

    bool decodeReference(const char *scope, const char *animationId,
                         AssetData::AnimationRef &reference, bool &selected) const
    {
        AssetData::AnimationRef parsed = {};
        if (!AssetData::parseAnimationRef(scope, animationId, manifest_, parsed) ||
            (bundleReader_ != nullptr &&
             !AssetData::animationReferenceExists(*bundleReader_, parsed)))
            return false;
        selected = parsed.speciesSlot == speciesSlot_ && parsed.outfitSlot == outfitSlot_;
        if (selected)
            reference = parsed;
        return true;
    }

    bool validActions() const
    {
        bool affectedSlots[kMaxPetBehaviorActions][kPetBehaviorSlotCount] = {};
        if (!validActionConditions() || !validRandomOutcomes())
            return false;
        for (uint8_t actionSlot = 0; actionSlot < kMaxPetBehaviorActions; ++actionSlot)
        {
            const PetBehaviorActionConfig &action = candidate.actions[actionSlot];
            if (!action.active)
                continue;
            if (action.mode == PetBehaviorActionMode::Standard &&
                (!action.hasFallbackAnimation || !action.animationPlayback.animation.valid()))
                return false;
            if (action.mode == PetBehaviorActionMode::ConditionalAnimation &&
                action.hasFallbackAnimation && !action.animationPlayback.animation.valid())
                return false;
            if (action.mode == PetBehaviorActionMode::RandomOutcome &&
                action.animationPlayback.animation.valid())
                return false;
        }
        for (uint8_t index = 0; index < candidate.actionEffectCount; ++index)
        {
            const PetBehaviorActionEffectConfig &effect = candidate.actionEffects[index];
            if (!effect.active || effect.actionSlot >= kMaxPetBehaviorActions ||
                !candidate.actions[effect.actionSlot].active ||
                candidate.actions[effect.actionSlot].mode == PetBehaviorActionMode::RandomOutcome ||
                effect.statSlot >= kPetBehaviorSlotCount ||
                !candidate.stats[effect.statSlot].active || affectedSlots[effect.actionSlot][effect.statSlot])
                return false;
            affectedSlots[effect.actionSlot][effect.statSlot] = true;
        }
        for (uint8_t slot = 0; slot < kPetBehaviorButtonCount; ++slot)
        {
            const PetBehaviorButtonConfig &button = candidate.buttons[slot];
            if (button.active && button.kind == PetBehaviorButtonKind::UserAction &&
                (button.actionSlot >= kMaxPetBehaviorActions || !candidate.actions[button.actionSlot].active))
                return false;
        }
        return true;
    }

    bool validRandomOutcomes() const
    {
        bool affectedSlots[kMaxPetBehaviorActions]
                          [kMaxPetBehaviorRandomOutcomesPerAction]
                          [kPetBehaviorSlotCount] = {};
        for (uint8_t actionSlot = 0; actionSlot < kMaxPetBehaviorActions; ++actionSlot)
        {
            const PetBehaviorActionConfig &action = candidate.actions[actionSlot];
            uint8_t outcomeCount = 0;
            for (uint8_t outcomeSlot = 0;
                 outcomeSlot < kMaxPetBehaviorRandomOutcomesPerAction;
                 ++outcomeSlot)
            {
                const PetBehaviorRandomOutcomeConfig &outcome =
                    candidate.randomOutcomes[actionSlot][outcomeSlot];
                if (!outcome.active)
                    continue;
                if (!action.active || action.mode != PetBehaviorActionMode::RandomOutcome ||
                    outcomeSlot != outcomeCount || outcome.weight == 0 || outcome.weight > 100 ||
                    !outcome.animationPlayback.animation.valid() ||
                    outcome.animationPlayback.playbackCount == 0 ||
                    outcome.animationPlayback.playbackCount > 5)
                    return false;
                ++outcomeCount;
            }
            if (action.active && action.mode == PetBehaviorActionMode::RandomOutcome)
            {
                if (outcomeCount < kMinPetBehaviorRandomOutcomesPerAction ||
                    outcomeCount > kMaxPetBehaviorRandomOutcomesPerAction)
                    return false;
            }
            else if (outcomeCount != 0)
            {
                return false;
            }
        }

        for (uint16_t index = 0; index < candidate.randomOutcomeEffectCount; ++index)
        {
            const PetBehaviorRandomOutcomeEffectConfig &effect = candidate.randomOutcomeEffects[index];
            if (!effect.active || effect.actionSlot >= kMaxPetBehaviorActions ||
                effect.outcomeSlot >= kMaxPetBehaviorRandomOutcomesPerAction ||
                !candidate.actions[effect.actionSlot].active ||
                candidate.actions[effect.actionSlot].mode != PetBehaviorActionMode::RandomOutcome ||
                !candidate.randomOutcomes[effect.actionSlot][effect.outcomeSlot].active ||
                effect.statSlot >= kPetBehaviorSlotCount || !candidate.stats[effect.statSlot].active ||
                affectedSlots[effect.actionSlot][effect.outcomeSlot][effect.statSlot])
                return false;
            affectedSlots[effect.actionSlot][effect.outcomeSlot][effect.statSlot] = true;
        }
        return true;
    }

    bool conditionsCoverDomain(const PetBehaviorActionConditionConfig *const *conditions,
                               uint8_t count,
                               int64_t domainMinimum,
                               int64_t domainMaximum) const
    {
        PetBehaviorActionConditionInterval intervals[kMaxPetBehaviorActionConditionsPerAction] = {};
        uint8_t intervalCount = 0;
        for (uint8_t index = 0; index < count; ++index)
        {
            PetBehaviorActionConditionInterval interval = {};
            if (!petBehaviorActionConditionInterval(
                    conditions[index]->comparison,
                    conditions[index]->threshold,
                    domainMinimum,
                    domainMaximum,
                    interval))
                continue;
            uint8_t position = intervalCount;
            while (position > 0 && intervals[position - 1].minimum > interval.minimum)
            {
                intervals[position] = intervals[position - 1];
                --position;
            }
            intervals[position] = interval;
            ++intervalCount;
        }
        if (intervalCount == 0 || intervals[0].minimum > domainMinimum)
            return false;

        int64_t coveredThrough = intervals[0].maximum;
        for (uint8_t index = 1; index < intervalCount && coveredThrough < domainMaximum; ++index)
        {
            if (intervals[index].minimum > coveredThrough + 1)
                return false;
            if (intervals[index].maximum > coveredThrough)
                coveredThrough = intervals[index].maximum;
        }
        return coveredThrough >= domainMaximum;
    }

    bool validActionConditions() const
    {
        for (uint8_t actionSlot = 0; actionSlot < kMaxPetBehaviorActions; ++actionSlot)
        {
            const PetBehaviorActionConfig &action = candidate.actions[actionSlot];
            const PetBehaviorActionConditionConfig *conditions[kMaxPetBehaviorActionConditionsPerAction] = {};
            uint8_t conditionCount = 0;
            for (uint8_t index = 0; index < candidate.actionConditionCount; ++index)
            {
                const PetBehaviorActionConditionConfig &condition = candidate.actionConditions[index];
                if (!condition.active || condition.actionSlot >= kMaxPetBehaviorActions)
                    return false;
                if (condition.actionSlot != actionSlot)
                    continue;
                if (!action.active || action.mode != PetBehaviorActionMode::ConditionalAnimation ||
                    conditionCount >= kMaxPetBehaviorActionConditionsPerAction)
                    return false;
                for (uint8_t existing = 0; existing < conditionCount; ++existing)
                {
                    if (conditions[existing]->priority == condition.priority)
                        return false;
                }
                if (condition.source == PetBehaviorActionConditionSource::PetStat &&
                    (condition.statSlot >= kPetBehaviorSlotCount || !candidate.stats[condition.statSlot].active))
                    return false;
                conditions[conditionCount++] = &condition;
            }

            if (!action.active)
                continue;
            if (action.mode == PetBehaviorActionMode::Standard ||
                action.mode == PetBehaviorActionMode::RandomOutcome)
            {
                if (conditionCount != 0)
                    return false;
                continue;
            }
            if (conditionCount == 0)
                return false;
            if (action.hasFallbackAnimation)
                continue;

            const PetBehaviorActionConditionConfig &first = *conditions[0];
            for (uint8_t index = 1; index < conditionCount; ++index)
            {
                if (conditions[index]->source != first.source ||
                    (first.source == PetBehaviorActionConditionSource::PetStat &&
                     conditions[index]->statSlot != first.statSlot))
                    return false;
            }
            const int64_t domainMinimum = first.source == PetBehaviorActionConditionSource::StageDays
                                              ? 0
                                              : candidate.stats[first.statSlot].minValue;
            const int64_t domainMaximum = first.source == PetBehaviorActionConditionSource::StageDays
                                              ? static_cast<int64_t>(UINT32_MAX)
                                              : candidate.stats[first.statSlot].maxValue;
            if (!conditionsCoverDomain(conditions, conditionCount, domainMinimum, domainMaximum))
                return false;
        }
        return true;
    }

    bool validStatusConditions() const
    {
        for (uint8_t setSlot = 0; setSlot < candidate.statusSets.count; ++setSlot)
        {
            const StatusSetConfig &set = candidate.statusSets.sets[setSlot];
            if (!set.animation.valid())
                return false;
            for (uint8_t conditionSlot = 0; conditionSlot < set.conditionCount; ++conditionSlot)
            {
                const StatusSetCondition &condition = set.conditions[conditionSlot];
                if (condition.source == StatusConditionSource::StageDays)
                    continue;
                if (condition.source != StatusConditionSource::PetStat ||
                    condition.statSlot >= kPetBehaviorSlotCount ||
                    !candidate.stats[condition.statSlot].active)
                {
                    return false;
                }
            }
        }
        return true;
    }

#if ENABLE_GUESS_GAME
    bool validGuessEffects() const
    {
        uint8_t outcomeCounts[kPetBehaviorGuessOutcomeCount] = {};
        bool affectedSlots[kPetBehaviorGuessOutcomeCount][kPetBehaviorSlotCount] = {};
        for (uint8_t index = 0; index < candidate.guessEffectCount; ++index)
        {
            const PetBehaviorGuessEffectConfig &effect = candidate.guessEffects[index];
            const uint8_t outcome = static_cast<uint8_t>(effect.outcome);
            if (!effect.active || outcome >= kPetBehaviorGuessOutcomeCount ||
                effect.statSlot >= kPetBehaviorSlotCount || !candidate.stats[effect.statSlot].active ||
                affectedSlots[outcome][effect.statSlot] || outcomeCounts[outcome] >= kMaxPetBehaviorStats)
                return false;
            affectedSlots[outcome][effect.statSlot] = true;
            ++outcomeCounts[outcome];
        }
        return true;
    }
#endif

    bool decodeStatusHeader(const SdTextRecord &record)
    {
        if (record.fieldCount != 2 || strcmp(record.fields[1], "1") != 0 || statusSeen)
            return false;
        statusSeen = true;
        return true;
    }

    bool decodeStatusSet(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        if (record.fieldCount != 2 ||
            !parseSlot(record.fields[1], "set", kMaxStatusSets, slot))
            return false;
        if (candidate.statusSets.count <= slot)
            candidate.statusSets.count = static_cast<uint8_t>(slot + 1);
        return true;
    }

    bool decodeStatusAnimation(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        AssetData::AnimationRef reference = {};
        bool selected = false;
        if (record.fieldCount != 4 ||
            !parseSlot(record.fields[1], "set", kMaxStatusSets, slot) ||
            !decodeReference(record.fields[2], record.fields[3], reference, selected))
            return false;
        if (!selected)
            return true;
        StatusSetConfig &set = candidate.statusSets.sets[slot];
        if (set.animation.valid())
            return false;
        set.animation = reference;
        return true;
    }

    bool decodeStatusCondition(const SdTextRecord &record)
    {
        uint8_t setSlot = 0;
        uint8_t conditionSlot = 0;
        uint32_t levels = 0;
        int32_t minValue = 0;
        int32_t maxValue = 0;
        if (record.fieldCount != 7 ||
            !parseSlot(record.fields[1], "set", kMaxStatusSets, setSlot) ||
            !parseSlot(record.fields[2], "condition", kMaxStatusConditions, conditionSlot) ||
            !parseUnsigned(record.fields[4], UINT8_MAX, levels) ||
            !parseSigned32(record.fields[5], minValue) ||
            !parseSigned32(record.fields[6], maxValue))
            return false;
        StatusSetConfig &set = candidate.statusSets.sets[setSlot];
        StatusSetCondition &condition = set.conditions[conditionSlot];
        if (strcmp(record.fields[3], "stage_days") == 0)
        {
            condition.source = StatusConditionSource::StageDays;
            condition.statSlot = 0;
        }
        else if (parseSlot(record.fields[3], "custom", kPetBehaviorSlotCount,
                           condition.statSlot))
        {
            condition.source = StatusConditionSource::PetStat;
        }
        else
            return false;
        condition.levels = static_cast<uint8_t>(levels);
        condition.minValue = minValue;
        condition.maxValue = maxValue;
        if (set.conditionCount <= conditionSlot)
            set.conditionCount = static_cast<uint8_t>(conditionSlot + 1);
        return true;
    }

    bool decodePetBehaviorHeader(const SdTextRecord &record)
    {
        if (record.fieldCount != 3 || strcmp(record.fields[1], "4") != 0 || identitySeen ||
            !parseHex32(record.fields[2], candidate.schemaFingerprint))
            return false;
        identitySeen = true;
        return true;
    }

    bool decodeStat(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        int16_t initialValue = 0;
        int16_t minValue = 0;
        int16_t maxValue = 0;
        int16_t dailyChange = 0;
        if (record.fieldCount != 6 || !parseSlot(record.fields[1], "custom", kPetBehaviorSlotCount, slot) ||
            !parseSigned16(record.fields[2], initialValue) || !parseSigned16(record.fields[3], minValue) ||
            !parseSigned16(record.fields[4], maxValue) || !parseSigned16(record.fields[5], dailyChange))
            return false;
        PetBehaviorStatConfig &stat = candidate.stats[slot];
        if (!stat.active)
            ++candidate.statCount;
        stat.active = true;
        stat.initialValue = initialValue;
        stat.minValue = minValue;
        stat.maxValue = maxValue;
        stat.dailyChange = dailyChange;
        return true;
    }

    bool decodeIdle(const SdTextRecord &record)
    {
        AssetData::AnimationRef reference = {};
        bool selected = false;
        if (record.fieldCount != 3 ||
            !decodeReference(record.fields[1], record.fields[2], reference, selected))
            return false;
        if (!selected)
            return true;
        if (idleSeen)
            return false;
        candidate.idleAnimation = reference;
        idleSeen = true;
        return true;
    }

    bool decodeIdleTrigger(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint8_t statSlot = 0;
        int16_t threshold = 0;
        if (record.fieldCount != 7 ||
            !parseSlot(record.fields[1], "trigger", kMaxPetBehaviorIdleTriggers, slot) ||
            !parseSlot(record.fields[2], "custom", kPetBehaviorSlotCount, statSlot) ||
            !parseSigned16(record.fields[4], threshold))
            return false;
        PetBehaviorIdleTriggerOperator comparison;
        if (strcmp(record.fields[3], "<") == 0)
            comparison = PetBehaviorIdleTriggerOperator::LessThan;
        else if (strcmp(record.fields[3], ">") == 0)
            comparison = PetBehaviorIdleTriggerOperator::GreaterThan;
        else
            return false;
        AssetData::AnimationRef reference = {};
        bool selected = false;
        if (!decodeReference(record.fields[5], record.fields[6], reference, selected))
            return false;
        if (!selected)
            return true;
        PetBehaviorIdleTriggerConfig &trigger = candidate.idleTriggers[slot];
        if (trigger.active)
            return false;
        if (!trigger.active)
            ++candidate.idleTriggerCount;
        trigger.active = true;
        trigger.statSlot = statSlot;
        trigger.comparison = comparison;
        trigger.threshold = threshold;
        trigger.animation = reference;
        return true;
    }

    bool decodeAction(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint32_t playbackCount = 0;
        uint32_t suspendDailyChangeDays = 0;
        if (record.fieldCount != 5 ||
            !parseSlot(record.fields[1], "action", kMaxPetBehaviorActions, slot) ||
            !parseUnsigned(record.fields[3], 5U, playbackCount) ||
            !parseUnsigned(record.fields[4], UINT8_MAX, suspendDailyChangeDays))
            return false;
        PetBehaviorActionConfig &action = candidate.actions[slot];
        if (action.active)
            return false;
        if (strcmp(record.fields[2], "standard") == 0)
        {
            if (playbackCount == 0)
                return false;
            action.mode = PetBehaviorActionMode::Standard;
            action.hasFallbackAnimation = false;
        }
        else if (strcmp(record.fields[2], "conditional_animation") == 0)
        {
            action.mode = PetBehaviorActionMode::ConditionalAnimation;
            action.hasFallbackAnimation = false;
        }
        else if (strcmp(record.fields[2], "random_outcome") == 0)
        {
            action.mode = PetBehaviorActionMode::RandomOutcome;
            action.hasFallbackAnimation = false;
        }
        else
        {
            return false;
        }
        ++candidate.actionCount;
        action.active = true;
        action.animationPlayback.playbackCount = static_cast<uint8_t>(playbackCount);
        action.suspendDailyChangeDays = static_cast<uint8_t>(suspendDailyChangeDays);
        return true;
    }

    bool decodeActionAnimation(const SdTextRecord &record)
    {
        uint8_t actionSlot = 0;
        AssetData::AnimationRef reference = {};
        bool selected = false;
        if (record.fieldCount != 4 ||
            !parseSlot(record.fields[1], "action", kMaxPetBehaviorActions, actionSlot) ||
            !decodeReference(record.fields[2], record.fields[3], reference, selected))
            return false;
        if (!selected)
            return true;
        PetBehaviorActionConfig &action = candidate.actions[actionSlot];
        if (!action.active || action.mode == PetBehaviorActionMode::RandomOutcome ||
            action.animationPlayback.animation.valid())
            return false;
        action.animationPlayback.animation = reference;
        action.hasFallbackAnimation = true;
        return true;
    }

    bool decodeActionOutcome(const SdTextRecord &record)
    {
        uint8_t actionSlot = 0;
        uint8_t outcomeSlot = 0;
        uint32_t weight = 0;
        uint32_t playbackCount = 0;
        if (record.fieldCount != 7 ||
            !parseSlot(record.fields[1], "action", kMaxPetBehaviorActions, actionSlot) ||
            !parseSlot(record.fields[2], "outcome", kMaxPetBehaviorRandomOutcomesPerAction, outcomeSlot) ||
            !parseUnsigned(record.fields[3], 100U, weight) || weight == 0 ||
            !parseUnsigned(record.fields[6], 5U, playbackCount) || playbackCount == 0)
            return false;
        AssetData::AnimationRef reference = {};
        bool selected = false;
        if (!decodeReference(record.fields[4], record.fields[5], reference, selected))
            return false;
        if (!selected)
            return true;
        PetBehaviorRandomOutcomeConfig &outcome = candidate.randomOutcomes[actionSlot][outcomeSlot];
        if (outcome.active)
            return false;
        outcome.active = true;
        outcome.weight = static_cast<uint8_t>(weight);
        outcome.animationPlayback.animation = reference;
        outcome.animationPlayback.playbackCount = static_cast<uint8_t>(playbackCount);
        return true;
    }

    bool decodeActionCondition(const SdTextRecord &record)
    {
        uint8_t actionSlot = 0;
        uint32_t priority = 0;
        int32_t threshold = 0;
        uint32_t playbackCount = 0;
        if (record.fieldCount != 9 ||
            !parseSlot(record.fields[1], "action", kMaxPetBehaviorActions, actionSlot) ||
            !parseUnsigned(record.fields[2], UINT8_MAX, priority) ||
            !parseSigned32(record.fields[5], threshold) ||
            !parseUnsigned(record.fields[8], 5U, playbackCount) || playbackCount == 0)
            return false;
        AssetData::AnimationRef reference = {};
        bool selected = false;
        if (!decodeReference(record.fields[6], record.fields[7], reference, selected))
            return false;
        if (!selected)
            return true;
        if (candidate.actionConditionCount >= kMaxPetBehaviorActionConditions)
            return false;
        PetBehaviorActionConditionConfig &condition =
            candidate.actionConditions[candidate.actionConditionCount];
        if (strcmp(record.fields[3], "stage_days") == 0)
        {
            condition.source = PetBehaviorActionConditionSource::StageDays;
        }
        else if (parseSlot(record.fields[3], "custom", kPetBehaviorSlotCount, condition.statSlot))
        {
            condition.source = PetBehaviorActionConditionSource::PetStat;
        }
        else
        {
            return false;
        }
        if (!parsePetBehaviorActionConditionOperator(record.fields[4], condition.comparison))
            return false;

        condition.active = true;
        condition.actionSlot = actionSlot;
        condition.priority = static_cast<uint8_t>(priority);
        condition.threshold = threshold;
        condition.animationPlayback.animation = reference;
        condition.animationPlayback.playbackCount = static_cast<uint8_t>(playbackCount);
        ++candidate.actionConditionCount;
        return true;
    }

    bool decodeActionEffect(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint8_t actionSlot = 0;
        uint8_t statSlot = 0;
        int16_t value = 0;
        if (record.fieldCount != 6 ||
            !parseSlot(record.fields[1], "effect", kMaxPetBehaviorActionEffects, slot) ||
            !parseSlot(record.fields[2], "action", kMaxPetBehaviorActions, actionSlot) ||
            !parseSlot(record.fields[4], "custom", kPetBehaviorSlotCount, statSlot) ||
            !parseSigned16(record.fields[5], value))
            return false;
        PetBehaviorEffectOperation operation;
        if (!parseEffectOperation(record.fields[3], operation))
            return false;
        PetBehaviorActionEffectConfig &effect = candidate.actionEffects[slot];
        if (effect.active)
            return false;
        ++candidate.actionEffectCount;
        effect.active = true;
        effect.actionSlot = actionSlot;
        effect.statSlot = statSlot;
        effect.operation = operation;
        effect.value = value;
        return true;
    }

    bool decodeActionOutcomeEffect(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint8_t actionSlot = 0;
        uint8_t outcomeSlot = 0;
        uint8_t statSlot = 0;
        int16_t value = 0;
        if (record.fieldCount != 7 ||
            !parseSlot(record.fields[1], "outcome_effect", kMaxPetBehaviorRandomOutcomeEffects, slot) ||
            !parseSlot(record.fields[2], "action", kMaxPetBehaviorActions, actionSlot) ||
            !parseSlot(record.fields[3], "outcome", kMaxPetBehaviorRandomOutcomesPerAction, outcomeSlot) ||
            !parseSlot(record.fields[5], "custom", kPetBehaviorSlotCount, statSlot) ||
            !parseSigned16(record.fields[6], value))
            return false;

        PetBehaviorEffectOperation operation;
        if (!parseEffectOperation(record.fields[4], operation))
            return false;

        PetBehaviorRandomOutcomeEffectConfig &effect = candidate.randomOutcomeEffects[slot];
        if (effect.active)
            return false;
        ++candidate.randomOutcomeEffectCount;
        effect.active = true;
        effect.actionSlot = actionSlot;
        effect.outcomeSlot = outcomeSlot;
        effect.statSlot = statSlot;
        effect.operation = operation;
        effect.value = value;
        return true;
    }

#if ENABLE_GUESS_GAME
    bool decodeGuessEffect(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint8_t statSlot = 0;
        int16_t value = 0;
        if (record.fieldCount != 6 ||
            !parseSlot(record.fields[1], "guess_effect", kMaxPetBehaviorGuessEffects, slot) ||
            !parseSlot(record.fields[4], "custom", kPetBehaviorSlotCount, statSlot) ||
            !parseSigned16(record.fields[5], value))
            return false;

        PetBehaviorGuessOutcome outcome;
        if (strcmp(record.fields[2], "round_correct") == 0)
            outcome = PetBehaviorGuessOutcome::RoundCorrect;
        else if (strcmp(record.fields[2], "round_wrong") == 0)
            outcome = PetBehaviorGuessOutcome::RoundWrong;
        else if (strcmp(record.fields[2], "game_win") == 0)
            outcome = PetBehaviorGuessOutcome::GameWin;
        else if (strcmp(record.fields[2], "game_loss") == 0)
            outcome = PetBehaviorGuessOutcome::GameLoss;
        else
            return false;

        PetBehaviorEffectOperation operation;
        if (!parseEffectOperation(record.fields[3], operation))
            return false;

        PetBehaviorGuessEffectConfig &effect = candidate.guessEffects[slot];
        if (effect.active)
            return false;
        ++candidate.guessEffectCount;
        effect.active = true;
        effect.outcome = outcome;
        effect.statSlot = statSlot;
        effect.operation = operation;
        effect.value = value;
        return true;
    }
#endif

    bool decodeButton(const SdTextRecord &record)
    {
        uint32_t position = 0;
        if (record.fieldCount != 4 || !parseUnsigned(record.fields[1], kPetBehaviorButtonCount, position) ||
            position == 0)
            return false;
        PetBehaviorButtonConfig &button = candidate.buttons[position - 1];
        if (strcmp(record.fields[2], "empty") == 0)
        {
            if (record.fields[3][0] != '\0')
                return false;
            button.kind = PetBehaviorButtonKind::Empty;
        }
        else if (strcmp(record.fields[2], "user_action") == 0)
        {
            if (!parseSlot(record.fields[3], "action", kMaxPetBehaviorActions, button.actionSlot))
                return false;
            button.kind = PetBehaviorButtonKind::UserAction;
        }
        else if (strcmp(record.fields[2], "system_command") == 0)
        {
            RuntimeSystemCommandId runtimeId;
            if (!runtimeSystemCommandIdForToken(record.fields[3], runtimeId))
                return false;
            button.systemCommandId = runtimeId;
            button.kind = PetBehaviorButtonKind::SystemCommand;
        }
        else
            return false;
        if (!button.active)
            ++candidate.buttonCount;
        button.active = true;
        return true;
    }
};

bool decodeRecord(void *context, const SdTextRecord &record)
{
    return context != nullptr && static_cast<PetBehaviorDecoder *>(context)->process(record);
}

bool referenceForActiveScope(const AssetData::AnimationRef &reference,
                             uint8_t speciesSlot,
                             uint8_t outfitSlot)
{
    return reference.shared() ||
           (reference.speciesSlot == speciesSlot && reference.outfitSlot == outfitSlot);
}

FirmwarePlaybackRole flowPlaybackRole(const char *kind, uint8_t slot)
{
    if (strcmp(kind, "predict") == 0)
    {
        const FirmwarePlaybackRole values[] = {
            FirmwarePlaybackRole::PredAnim, FirmwarePlaybackRole::Predict1, FirmwarePlaybackRole::Predict2,
            FirmwarePlaybackRole::Predict3, FirmwarePlaybackRole::Predict4, FirmwarePlaybackRole::Predict5,
            FirmwarePlaybackRole::Predict6, FirmwarePlaybackRole::Predict7, FirmwarePlaybackRole::Predict8,
            FirmwarePlaybackRole::Predict9, FirmwarePlaybackRole::Predict10, FirmwarePlaybackRole::Predict11};
        return slot < sizeof(values) / sizeof(values[0]) ? values[slot] : FirmwarePlaybackRole::None;
    }
    if (strcmp(kind, "guess") == 0)
    {
        const FirmwarePlaybackRole values[] = {
            FirmwarePlaybackRole::GuessStart, FirmwarePlaybackRole::GuessItem1, FirmwarePlaybackRole::GuessItem2,
            FirmwarePlaybackRole::GuessItem3, FirmwarePlaybackRole::GuessItem4, FirmwarePlaybackRole::GuessLL,
            FirmwarePlaybackRole::GuessLR, FirmwarePlaybackRole::GuessRL, FirmwarePlaybackRole::GuessRR,
            FirmwarePlaybackRole::GuessWin, FirmwarePlaybackRole::GuessLoss, FirmwarePlaybackRole::GuessRight,
            FirmwarePlaybackRole::GuessWrong};
        return slot < sizeof(values) / sizeof(values[0]) ? values[slot] : FirmwarePlaybackRole::None;
    }
    if (strcmp(kind, "startup") == 0)
    {
        const FirmwarePlaybackRole values[] = {
            FirmwarePlaybackRole::Start, FirmwarePlaybackRole::StartIntro, FirmwarePlaybackRole::FirstStart};
        return slot < sizeof(values) / sizeof(values[0]) ? values[slot] : FirmwarePlaybackRole::None;
    }
    return FirmwarePlaybackRole::None;
}

class FlowDecoder
{
public:
    FlowDecoder(PetBehaviorConfig &config, BundleReader &bundleReader)
        : config_(config), bundleReader_(bundleReader) {}

    bool process(const SdTextRecord &record)
    {
        if (record.fieldOverflow || record.fieldCount == 0)
            return false;
        if (strcmp(record.fields[0], "asset_data") == 0)
        {
            if (assetDataSeen_ || record.fieldCount != 2 || strcmp(record.fields[1], "1") != 0)
                return false;
            assetDataSeen_ = true;
            return true;
        }
        if (strcmp(record.fields[0], "bundle_id") == 0)
        {
            AssetData::BundleId bundleId = {};
            if (!assetDataSeen_ || bundleSeen_ || record.fieldCount != 2 ||
                !AssetData::parseBundleId(record.fields[1], bundleId) ||
                !AssetData::sameBundleId(bundleId, config_.assetManifest.bundleId))
                return false;
            bundleSeen_ = true;
            return true;
        }
        if (!assetDataSeen_ || !bundleSeen_)
            return false;
        if (strcmp(record.fields[0], "guess_game") == 0)
            return record.fieldCount == 5;
        if (strcmp(record.fields[0], "shared_asset") == 0)
            return decodeShared(record);
        if (strcmp(record.fields[0], "flow") == 0)
            return decodeFlow(record);
        return false;
    }

    bool complete() const { return assetDataSeen_ && bundleSeen_ && batterySeen_; }

private:
    bool decodeShared(const SdTextRecord &record)
    {
        AssetData::AnimationRef reference = {};
        if (record.fieldCount != 4 || strcmp(record.fields[1], "battery") != 0 ||
            !AssetData::parseAnimationRef(record.fields[2], record.fields[3],
                                          config_.assetManifest, reference) ||
            !reference.shared() ||
            !AssetData::animationReferenceExists(bundleReader_, reference) || batterySeen_)
            return false;
        config_.systemAnimations[static_cast<size_t>(FirmwarePlaybackRole::Battery)] = reference;
        batterySeen_ = true;
        return true;
    }

    bool decodeFlow(const SdTextRecord &record)
    {
        uint32_t slot = 0;
        AssetData::AnimationRef reference = {};
        if (record.fieldCount != 5 || !parseUnsigned(record.fields[2], 12U, slot) ||
            !AssetData::parseAnimationRef(record.fields[3], record.fields[4],
                                          config_.assetManifest, reference) ||
            !AssetData::animationReferenceExists(bundleReader_, reference))
            return false;
        const FirmwarePlaybackRole playbackRole = flowPlaybackRole(record.fields[1], static_cast<uint8_t>(slot));
        if (playbackRole == FirmwarePlaybackRole::None)
            return false;
        if (!referenceForActiveScope(reference, config_.activeSpeciesSlot, config_.activeOutfitSlot))
            return true;
        AssetData::AnimationRef &destination =
            config_.systemAnimations[static_cast<size_t>(playbackRole)];
        if (destination.valid())
            return false;
        destination = reference;
        return true;
    }

    PetBehaviorConfig &config_;
    BundleReader &bundleReader_;
    bool assetDataSeen_ = false;
    bool bundleSeen_ = false;
    bool batterySeen_ = false;
};

bool decodeFlowRecord(void *context, const SdTextRecord &record)
{
    return context != nullptr && static_cast<FlowDecoder *>(context)->process(record);
}

class LayoutDecoder
{
public:
    LayoutDecoder(PetBehaviorConfig &config, BundleReader &bundleReader)
        : config_(config), bundleReader_(bundleReader) {}

    bool process(const SdTextRecord &record)
    {
        if (record.fieldOverflow || record.fieldCount == 0)
            return false;
        if (strcmp(record.fields[0], "asset_data") == 0)
        {
            if (assetDataSeen_ || record.fieldCount != 2 || strcmp(record.fields[1], "1") != 0)
                return false;
            assetDataSeen_ = true;
            return true;
        }
        if (strcmp(record.fields[0], "bundle_id") == 0)
        {
            AssetData::BundleId bundleId = {};
            if (!assetDataSeen_ || bundleSeen_ || record.fieldCount != 2 ||
                !AssetData::parseBundleId(record.fields[1], bundleId) ||
                !AssetData::sameBundleId(bundleId, config_.assetManifest.bundleId))
                return false;
            bundleSeen_ = true;
            return true;
        }
        if (!assetDataSeen_ || !bundleSeen_)
            return false;
        if (strcmp(record.fields[0], "button_count") == 0)
        {
            uint32_t count = 0;
            if (buttonCountSeen_ || record.fieldCount != 2 ||
                !parseUnsigned(record.fields[1], kPetBehaviorButtonCount, count) ||
                count != kPetBehaviorButtonCount)
                return false;
            buttonCountSeen_ = true;
            return true;
        }
        if (strcmp(record.fields[0], "action_layout") == 0)
        {
            uint32_t playbackRole = 0;
            uint32_t versionIndex = 0;
            if (record.fieldCount != 3 ||
                !parseUnsigned(record.fields[1], kFirmwarePlaybackRoleCount - 1U, playbackRole) ||
                playbackRole == 0 ||
                !parseUnsigned(record.fields[2], AssetData::kMaxVersions - 1U, versionIndex) ||
                versionIndex == 0 || versionIndex != nextLayoutVersion_ ||
                config_.actionLayoutVersions[playbackRole] != 0)
                return false;
            config_.actionLayoutVersions[playbackRole] = static_cast<uint8_t>(versionIndex);
            ++nextLayoutVersion_;
            return true;
        }
        if (strcmp(record.fields[0], "layout") != 0 || record.fieldCount != 4)
            return false;
        AssetData::AnimationRef reference = {};
        if (!AssetData::parseAnimationRef(record.fields[2], record.fields[3],
                                          config_.assetManifest, reference) ||
            !reference.shared() ||
            !AssetData::animationReferenceExists(bundleReader_, reference))
            return false;
        AssetData::AnimationRef *destination = nullptr;
        if (strcmp(record.fields[1], "unselected") == 0)
            destination = &config_.layoutUnselected;
        else if (strcmp(record.fields[1], "selected") == 0)
            destination = &config_.layoutSelected;
        else
            return false;
        if (destination->valid())
            return false;
        *destination = reference;
        return true;
    }

    bool complete() const
    {
        if (!assetDataSeen_ || !bundleSeen_ || !buttonCountSeen_ ||
            !config_.layoutUnselected.valid() || !config_.layoutSelected.valid())
            return false;
        for (size_t role = 0; role < kFirmwarePlaybackRoleCount; ++role)
        {
            const uint8_t version = config_.actionLayoutVersions[role];
            if (version != 0 &&
                (!AssetData::animationReferenceExists(bundleReader_, config_.layoutUnselected, version) ||
                 !AssetData::animationReferenceExists(bundleReader_, config_.layoutSelected, version)))
                return false;
        }
        return true;
    }

private:
    PetBehaviorConfig &config_;
    BundleReader &bundleReader_;
    bool assetDataSeen_ = false;
    bool bundleSeen_ = false;
    bool buttonCountSeen_ = false;
    uint8_t nextLayoutVersion_ = 1;
};

bool decodeLayoutRecord(void *context, const SdTextRecord &record)
{
    return context != nullptr && static_cast<LayoutDecoder *>(context)->process(record);
}

void copyLoadError(char *destination, size_t capacity,
                   const char *fallback, const BundleReader *reader = nullptr)
{
    if (destination == nullptr || capacity == 0)
        return;
    const char *pack = reader == nullptr ? nullptr : reader->firstErrorResource();
    const char *resource = pack != nullptr && pack[0] != '\0' ? pack : fallback;
    strncpy(destination, resource, capacity - 1);
    destination[capacity - 1] = '\0';
}
} // namespace

bool parsePetBehaviorContract(const char *contractText,
                              const AssetData::RuntimeManifest &manifest,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config)
{
    config = {};
    PetBehaviorDecoder decoder(manifest, speciesSlot, outfitSlot);
    return parseSdTextRecords(contractText, kMaxPetBehaviorContractBytes, kRuntimeContractIdentity,
                              kRuntimeContractVersion, decodeRecord, &decoder) && decoder.complete(config);
}

bool loadPetBehaviorContract(SdFat *sd,
                             uint8_t speciesSlot,
                             uint8_t outfitSlot,
                             PetBehaviorConfig &config,
                             char *errorResource,
                             size_t errorResourceCapacity)
{
    config = {};
    if (errorResource != nullptr && errorResourceCapacity != 0)
        errorResource[0] = '\0';
    AssetData::RuntimeManifest manifest = {};
    if (speciesSlot == 0 || outfitSlot == 0 || !AssetData::loadRuntimeManifest(sd, manifest))
    {
        copyLoadError(errorResource, errorResourceCapacity, "asset_manifest");
        return false;
    }
    uint8_t verificationScratch[AssetData::kVerificationScratchBytes] = {};
    BundleReader bundleReader(sd, verificationScratch, sizeof(verificationScratch));
    if (!bundleReader.configureBundle(manifest.bundleId))
    {
        copyLoadError(errorResource, errorResourceCapacity, "asset data", &bundleReader);
        return false;
    }
    PetBehaviorDecoder decoder(manifest, speciesSlot, outfitSlot, &bundleReader, true);
    if (!loadSdTextRecords(sd, kRuntimeContractPath, kMaxPetBehaviorContractBytes,
                           kRuntimeContractIdentity, kRuntimeContractVersion, decodeRecord, &decoder) ||
        !decoder.complete(config))
    {
        copyLoadError(errorResource, errorResourceCapacity, "runtime_contract", &bundleReader);
        return false;
    }
    FlowDecoder flow(config, bundleReader);
    if (!loadSdTextRecords(sd, kFlowContractPath, kMaxAuxiliaryContractBytes,
                           "flow_contract", "1", decodeFlowRecord, &flow) || !flow.complete())
    {
        copyLoadError(errorResource, errorResourceCapacity, "flow_contract", &bundleReader);
        return false;
    }
    LayoutDecoder layout(config, bundleReader);
    const bool loaded = loadSdTextRecords(sd, kLayoutContractPath, kMaxAuxiliaryContractBytes,
                                          "layout_contract", "1", decodeLayoutRecord, &layout) &&
                        layout.complete();
    if (!loaded)
    {
        copyLoadError(errorResource, errorResourceCapacity, "layout_contract", &bundleReader);
        return false;
    }
    if (!loadRuntimeTableBehavior(sd, manifest, speciesSlot, outfitSlot, config))
    {
        config = {};
        copyLoadError(errorResource, errorResourceCapacity, "runtime");
        return false;
    }
    return true;
}
