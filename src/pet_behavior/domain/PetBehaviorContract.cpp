#include "pet_behavior/domain/PetBehaviorContract.h"

#include <limits.h>
#include <string.h>
#include "shared/sd/SdTextRecordReader.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"

namespace
{
constexpr const char *kRuntimeContractPath = "/runtime_contract.txt";
constexpr const char *kRuntimeContractIdentity = "runtime_contract";
constexpr const char *kRuntimeContractVersion = "3";

bool parseUnsigned(const char *text, uint32_t maximum, uint32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;
    uint32_t parsed = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
        if (parsed > (maximum - digit) / 10U)
            return false;
        parsed = parsed * 10U + digit;
    }
    value = parsed;
    return true;
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
        else if (*cursor >= 'A' && *cursor <= 'F')
            digit = static_cast<uint8_t>(*cursor - 'A' + 10);
        else
            return false;
        parsed = (parsed << 4U) | digit;
    }
    value = parsed;
    return true;
}

bool parseSlot(const char *token, const char *prefix, uint8_t capacity, uint8_t &slot)
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

class PetBehaviorDecoder
{
public:
    bool process(const SdTextRecord &record)
    {
        if (record.fieldCount == 0)
            return false;
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
        if (strcmp(record.fields[0], "action_effect") == 0)
            return decodeActionEffect(record);
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
        if (strcmp(record.fields[0], "status_condition") == 0)
            return decodeStatusCondition(record);
        return false;
    }

    bool complete(PetBehaviorConfig &destination) const
    {
        if (!identitySeen || !statusSeen || !idleSeen || candidate.buttonCount != kPetBehaviorButtonCount ||
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
    bool identitySeen = false;
    bool statusSeen = false;
    bool idleSeen = false;

    bool validActions() const
    {
        bool affectedSlots[kMaxPetBehaviorActions][kPetBehaviorSlotCount] = {};
        for (uint8_t index = 0; index < candidate.actionEffectCount; ++index)
        {
            const PetBehaviorActionEffectConfig &effect = candidate.actionEffects[index];
            if (!effect.active || effect.actionSlot >= kMaxPetBehaviorActions ||
                !candidate.actions[effect.actionSlot].active || effect.statSlot >= kPetBehaviorSlotCount ||
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

    bool validStatusConditions() const
    {
        const ActivePetBehaviorStatSlots activeSlots(candidate);
        for (uint8_t setSlot = 0; setSlot < candidate.statusSets.count; ++setSlot)
        {
            const StatusSetConfig &set = candidate.statusSets.sets[setSlot];
            for (uint8_t conditionSlot = 0; conditionSlot < set.conditionCount; ++conditionSlot)
            {
                const char *source = set.conditions[conditionSlot].source;
                if (strcmp(source, "stage_days") == 0)
                    continue;

                uint8_t statSlot = 0;
                if (!activeSlots.resolve(source, statSlot))
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
        if (record.fieldCount != 3 ||
            !parseSlot(record.fields[1], "set", kMaxStatusSets, slot))
            return false;
        StatusSetConfig &set = candidate.statusSets.sets[slot];
        if (!copyBounded(record.fields[2], set.animation, sizeof(set.animation)))
            return false;
        if (candidate.statusSets.count <= slot)
            candidate.statusSets.count = static_cast<uint8_t>(slot + 1);
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
        if (!copyBounded(record.fields[3], condition.source, sizeof(condition.source)))
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
        if (record.fieldCount != 3 || strcmp(record.fields[1], "3") != 0 || identitySeen ||
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
        if (record.fieldCount != 2 || idleSeen ||
            !copyBounded(record.fields[1], candidate.idleAnimation, sizeof(candidate.idleAnimation)))
            return false;
        idleSeen = true;
        return true;
    }

    bool decodeIdleTrigger(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint8_t statSlot = 0;
        int16_t threshold = 0;
        if (record.fieldCount != 6 ||
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
        PetBehaviorIdleTriggerConfig &trigger = candidate.idleTriggers[slot];
        if (!copyBounded(record.fields[5], trigger.animation, sizeof(trigger.animation)))
            return false;
        if (!trigger.active)
            ++candidate.idleTriggerCount;
        trigger.active = true;
        trigger.statSlot = statSlot;
        trigger.comparison = comparison;
        trigger.threshold = threshold;
        return true;
    }

    bool decodeAction(const SdTextRecord &record)
    {
        uint8_t slot = 0;
        uint32_t playbackCount = 0;
        uint32_t suspendDailyChangeDays = 0;
        if (record.fieldCount != 6 ||
            !parseSlot(record.fields[1], "action", kMaxPetBehaviorActions, slot) ||
            strcmp(record.fields[2], "standard") != 0 ||
            !parseUnsigned(record.fields[4], 5U, playbackCount) || playbackCount == 0 ||
            !parseUnsigned(record.fields[5], UINT8_MAX, suspendDailyChangeDays))
            return false;
        PetBehaviorActionConfig &action = candidate.actions[slot];
        if (action.active || !copyBounded(record.fields[3], action.animation, sizeof(action.animation)))
            return false;
        ++candidate.actionCount;
        action.active = true;
        action.mode = PetBehaviorActionMode::Standard;
        action.playbackCount = static_cast<uint8_t>(playbackCount);
        action.suspendDailyChangeDays = static_cast<uint8_t>(suspendDailyChangeDays);
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
        if (strcmp(record.fields[3], "change") == 0)
            operation = PetBehaviorEffectOperation::Change;
        else if (strcmp(record.fields[3], "set") == 0)
            operation = PetBehaviorEffectOperation::Set;
        else
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
        if (strcmp(record.fields[3], "change") == 0)
            operation = PetBehaviorEffectOperation::Change;
        else if (strcmp(record.fields[3], "set") == 0)
            operation = PetBehaviorEffectOperation::Set;
        else
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
            if (!copyBounded(record.fields[3], button.systemCommand, sizeof(button.systemCommand)))
                return false;
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
} // namespace

bool parsePetBehaviorContract(const char *contractText, PetBehaviorConfig &config)
{
    config = {};
    PetBehaviorDecoder decoder;
    return parseSdTextRecords(contractText, kMaxPetBehaviorContractBytes, kRuntimeContractIdentity,
                              kRuntimeContractVersion, decodeRecord, &decoder) && decoder.complete(config);
}

bool loadPetBehaviorContract(SdFat *sd, PetBehaviorConfig &config)
{
    config = {};
    PetBehaviorDecoder decoder;
    return loadSdTextRecords(sd, kRuntimeContractPath, kMaxPetBehaviorContractBytes,
                             kRuntimeContractIdentity, kRuntimeContractVersion, decodeRecord, &decoder) &&
           decoder.complete(config);
}
