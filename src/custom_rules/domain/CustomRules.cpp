#include "custom_rules/domain/CustomRules.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "animation/application/AnimationController.h"
#include "pet/application/PetActionController.h"
#include "pet/domain/Pet.h"
#include "shared/debug/DebugDisplay.h"
#include "shared/utils/TextBuffer.h"

#if ENABLE_CUSTOM_RULES

namespace
{
constexpr const char *kCustomRulesPath = "/custom_rules.txt";
constexpr size_t kLineSize = 128;
constexpr unsigned long kActionDurationMs = 2400;

void showLoadError(DebugDisplay *debug, uint16_t lineNumber, const char *reason)
{
    if (debug == nullptr)
        return;

    char detail[24] = {};
    if (lineNumber > 0)
    {
        TextBuffer text(detail, sizeof(detail));
        text.append("L");
        text.appendUnsigned(lineNumber);
        text.append(" ");
        text.append(reason);
    }
    else
    {
        TextBuffer text(detail, sizeof(detail));
        text.append(reason);
    }

    debug->showMessage("custom_rules", detail);
}

bool isSpace(char value)
{
    return value == ' ' || value == '\t';
}

char *trim(char *text)
{
    if (text == nullptr)
        return nullptr;

    while (isSpace(*text))
        ++text;

    char *end = text + strlen(text);
    while (end > text && (isSpace(*(end - 1)) || *(end - 1) == '\r' || *(end - 1) == '\n'))
        *--end = '\0';
    return text;
}

bool readLine(File &file, char *line, size_t lineSize)
{
    if (line == nullptr || lineSize == 0)
        return false;

    size_t index = 0;
    bool sawData = false;
    bool truncated = false;
    while (file.available())
    {
        const char value = static_cast<char>(file.read());
        sawData = true;
        if (value == '\n')
            break;
        if (value == '\r')
            continue;
        if (index + 1 < lineSize)
            line[index++] = value;
        else
            truncated = true;
    }
    line[index] = '\0';
    if (truncated)
    {
        char *content = trim(line);
        return content != nullptr && (content[0] == '\0' || content[0] == '#');
    }
    return sawData;
}

bool splitFields(char *line, char **fields, size_t expectedCount)
{
    if (line == nullptr || fields == nullptr || expectedCount == 0)
        return false;

    char *cursor = line;
    for (size_t index = 0; index < expectedCount; ++index)
    {
        fields[index] = trim(cursor);
        char *separator = strchr(cursor, '|');
        if (index + 1 == expectedCount)
        {
            if (separator != nullptr)
                return false;
        }
        else
        {
            if (separator == nullptr)
                return false;
            *separator = '\0';
            cursor = separator + 1;
        }

        if (fields[index] == nullptr || fields[index][0] == '\0')
            return false;
    }
    return true;
}

bool parseInt16(const char *text, int16_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;

    char *end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < INT16_MIN || parsed > INT16_MAX)
        return false;

    value = static_cast<int16_t>(parsed);
    return true;
}

bool parseSlot(const char *text, uint8_t &slot)
{
    if (text != nullptr && strncmp(text, "custom", 6) == 0 && text[6] >= '0' && text[6] <= '7' && text[7] == '\0')
    {
        slot = static_cast<uint8_t>(text[6] - '0');
        return true;
    }

    int16_t parsed = 0;
    if (!parseInt16(text, parsed) || parsed < 0 || parsed >= CustomRules::kStatCount)
        return false;
    slot = static_cast<uint8_t>(parsed);
    return true;
}

bool copyToken(char *destination, size_t destinationSize, const char *source)
{
    if (destination == nullptr || destinationSize == 0 || source == nullptr)
        return false;

    const size_t length = strlen(source);
    if (length == 0 || length >= destinationSize)
        return false;

    memcpy(destination, source, length + 1);
    return true;
}

bool customSlotFromKey(const char *key, uint8_t &slot)
{
    if (key == nullptr || strlen(key) != 7 || strncmp(key, "CUSTOM", 6) != 0 || key[6] < '0' || key[6] > '7')
        return false;
    slot = static_cast<uint8_t>(key[6] - '0');
    return true;
}

bool hasActionKey(const CustomRules::ActionRule *rules, uint8_t count, const char *key)
{
    for (uint8_t index = 0; index < count; ++index)
    {
        if (strcmp(rules[index].key, key) == 0)
            return true;
    }
    return false;
}

bool hasVariantEffectAnimation(const CustomRules::VariantEffect *effects, uint8_t count, const char *animation)
{
    for (uint8_t index = 0; index < count; ++index)
    {
        if (strcmp(effects[index].animation, animation) == 0)
            return true;
    }
    return false;
}
} // namespace

void CustomRules::clear()
{
    memset(stats, 0, sizeof(stats));
    memset(dailyRules, 0, sizeof(dailyRules));
    memset(actionRules, 0, sizeof(actionRules));
    memset(variantEffects, 0, sizeof(variantEffects));
    dailyRuleCount = 0;
    actionRuleCount = 0;
    variantEffectCount = 0;
    enabled = false;
}

bool CustomRules::load(SdFat *sd, DebugDisplay *debug)
{
    clear();
    if (sd == nullptr)
    {
        showLoadError(debug, 0, "sd is null");
        return false;
    }

    if (!sd->exists(kCustomRulesPath))
    {
        showLoadError(debug, 0, "missing file");
        return false;
    }

    File file = sd->open(kCustomRulesPath, FILE_READ);
    if (!file)
    {
        showLoadError(debug, 0, "open failed");
        return false;
    }

    bool valid = true;
    uint16_t lineNumber = 0;
    char line[kLineSize] = {};
    while (file.available() && valid)
    {
        ++lineNumber;
        if (!readLine(file, line, sizeof(line)))
        {
            showLoadError(debug, lineNumber, "line too long");
            valid = false;
            break;
        }

        char *content = trim(line);
        if (content[0] == '\0' || content[0] == '#')
            continue;

        char *separator = strchr(content, '|');
        if (separator == nullptr)
        {
            showLoadError(debug, lineNumber, "missing sep");
            valid = false;
            break;
        }

        *separator = '\0';
        const char *type = trim(content);
        char *row = separator + 1;
        if (strcmp(type, "stat") == 0)
        {
            char *fields[5] = {};
            if (!splitFields(row, fields, 5))
            {
                showLoadError(debug, lineNumber, "bad stat row");
                valid = false;
                break;
            }

            uint8_t slot = 0;
            int16_t initialValue = 0;
            int16_t minValue = 0;
            int16_t maxValue = 0;
            if (!parseSlot(fields[0], slot))
            {
                showLoadError(debug, lineNumber, "bad stat slot");
                valid = false;
            }
            else if (stats[slot].defined)
            {
                showLoadError(debug, lineNumber, "dup stat");
                valid = false;
            }
            else if (!copyToken(stats[slot].label, sizeof(stats[slot].label), fields[1]))
            {
                showLoadError(debug, lineNumber, "bad stat label");
                valid = false;
            }
            else if (!parseInt16(fields[2], initialValue) || !parseInt16(fields[3], minValue) ||
                     !parseInt16(fields[4], maxValue))
            {
                showLoadError(debug, lineNumber, "bad stat value");
                valid = false;
            }
            else if (minValue > initialValue || initialValue > maxValue)
            {
                showLoadError(debug, lineNumber, "bad stat range");
                valid = false;
            }
            else
            {
                stats[slot].defined = true;
                stats[slot].initialValue = initialValue;
                stats[slot].minValue = minValue;
                stats[slot].maxValue = maxValue;
            }
        }
        else if (strcmp(type, "daily") == 0)
        {
            char *fields[2] = {};
            if (!splitFields(row, fields, 2))
            {
                showLoadError(debug, lineNumber, "bad daily row");
                valid = false;
                break;
            }
            if (dailyRuleCount >= kMaxDailyRules)
            {
                showLoadError(debug, lineNumber, "too many daily");
                valid = false;
                break;
            }

            uint8_t slot = 0;
            int16_t delta = 0;
            if (!parseSlot(fields[0], slot))
            {
                showLoadError(debug, lineNumber, "bad daily slot");
                valid = false;
            }
            else if (!parseInt16(fields[1], delta))
            {
                showLoadError(debug, lineNumber, "bad daily value");
                valid = false;
            }
            else
            {
                dailyRules[dailyRuleCount++] = {slot, delta};
            }
        }
        else if (strcmp(type, "action") == 0)
        {
            char *fields[5] = {};
            if (!splitFields(row, fields, 5))
            {
                showLoadError(debug, lineNumber, "bad action row");
                valid = false;
                break;
            }
            if (actionRuleCount >= kMaxActionRules)
            {
                showLoadError(debug, lineNumber, "too many action");
                valid = false;
                break;
            }

            uint8_t targetSlot = 0;
            uint8_t requiredSlot = 0;
            int16_t requiredMin = 0;
            int16_t delta = 0;
            if (!customSlotFromKey(fields[0], targetSlot))
            {
                showLoadError(debug, lineNumber, "bad action key");
                valid = false;
            }
            else if (!parseSlot(fields[1], requiredSlot))
            {
                showLoadError(debug, lineNumber, "bad req slot");
                valid = false;
            }
            else if (!parseInt16(fields[2], requiredMin) || !parseInt16(fields[3], delta))
            {
                showLoadError(debug, lineNumber, "bad action val");
                valid = false;
            }
            else if (fields[4] == nullptr || fields[4][0] == '\0' || strlen(fields[4]) >= sizeof(actionRules[actionRuleCount].animation))
            {
                showLoadError(debug, lineNumber, "bad animation");
                valid = false;
            }
            else if (hasActionKey(actionRules, actionRuleCount, fields[0]))
            {
                showLoadError(debug, lineNumber, "dup action");
                valid = false;
            }
            else
            {
                ActionRule &rule = actionRules[actionRuleCount];
                valid = copyToken(rule.key, sizeof(rule.key), fields[0]);
                if (!valid)
                {
                    showLoadError(debug, lineNumber, "bad action key");
                }
                else
                {
                    rule.targetSlot = targetSlot;
                    rule.requiredSlot = requiredSlot;
                    rule.requiredMin = requiredMin;
                    rule.delta = delta;
                    valid = copyToken(rule.animation, sizeof(rule.animation), fields[4]);
                    if (!valid)
                    {
                        showLoadError(debug, lineNumber, "bad animation");
                        break;
                    }
                    ++actionRuleCount;
                }
            }
        }
        else if (strcmp(type, "variant_effect") == 0)
        {
            char *fields[3] = {};
            if (!splitFields(row, fields, 3))
            {
                showLoadError(debug, lineNumber, "bad variant row");
                valid = false;
                break;
            }
            if (variantEffectCount >= kMaxVariantEffects)
            {
                showLoadError(debug, lineNumber, "too many variants");
                valid = false;
                break;
            }

            int16_t moodDelta = 0;
            if (strcmp(fields[1], "mood") != 0)
            {
                showLoadError(debug, lineNumber, "bad variant stat");
                valid = false;
            }
            else if (!parseInt16(fields[2], moodDelta))
            {
                showLoadError(debug, lineNumber, "bad variant value");
                valid = false;
            }
            else if (hasVariantEffectAnimation(variantEffects, variantEffectCount, fields[0]))
            {
                showLoadError(debug, lineNumber, "dup variant");
                valid = false;
            }
            else if (!copyToken(variantEffects[variantEffectCount].animation,
                                sizeof(variantEffects[variantEffectCount].animation),
                                fields[0]))
            {
                showLoadError(debug, lineNumber, "bad variant name");
                valid = false;
            }
            else
            {
                variantEffects[variantEffectCount].moodDelta = moodDelta;
                ++variantEffectCount;
            }
        }
        else
        {
            showLoadError(debug, lineNumber, "unknown type");
            valid = false;
        }
    }
    file.close();

    for (uint8_t index = 0; valid && index < dailyRuleCount; ++index)
    {
        if (!stats[dailyRules[index].slot].defined)
        {
            showLoadError(debug, 0, "daily undef stat");
            valid = false;
        }
    }
    for (uint8_t index = 0; valid && index < actionRuleCount; ++index)
    {
        if (!stats[actionRules[index].requiredSlot].defined || !stats[actionRules[index].targetSlot].defined)
        {
            showLoadError(debug, 0, "action undef stat");
            valid = false;
        }
    }

    if (!valid)
    {
        clear();
        return false;
    }

    enabled = true;
    return true;
}

bool CustomRules::isEnabled() const
{
    return enabled;
}

bool CustomRules::hasAction(const char *key) const
{
    return findAction(key) != nullptr;
}

void CustomRules::applyInitialValues(Pet &pet) const
{
    if (!enabled)
        return;

    for (uint8_t slot = 0; slot < kStatCount; ++slot)
    {
        if (stats[slot].defined)
            pet.setCustomStat(slot, stats[slot].initialValue);
    }
}

void CustomRules::clampValues(Pet &pet) const
{
    if (!enabled)
        return;

    for (uint8_t slot = 0; slot < kStatCount; ++slot)
    {
        if (!stats[slot].defined)
            continue;

        const int16_t currentValue = pet.customStat(slot);
        const int16_t clampedValue = currentValue < stats[slot].minValue
                                         ? stats[slot].minValue
                                         : (currentValue > stats[slot].maxValue ? stats[slot].maxValue : currentValue);
        pet.setCustomStat(slot, clampedValue);
    }
}

void CustomRules::applyDaily(PetActionController &petActions) const
{
    if (!enabled)
        return;

    for (uint8_t index = 0; index < dailyRuleCount; ++index)
    {
        const DailyRule &rule = dailyRules[index];
        const StatRule &stat = stats[rule.slot];
        petActions.changeCustomStatClamped(rule.slot, rule.delta, stat.minValue, stat.maxValue);
    }
}

bool CustomRules::applyVariantEffect(const char *animationName, PetActionController &petActions) const
{
    const VariantEffect *effect = findVariantEffect(animationName);
    if (effect == nullptr)
        return false;

    petActions.changeMood(effect->moodDelta);
    return true;
}

bool CustomRules::executeAction(const char *key, PetActionController &petActions, AnimationController &animations) const
{
    const ActionRule *rule = findAction(key);
    if (rule == nullptr || petActions.customStat(rule->requiredSlot) < rule->requiredMin)
        return false;

    const StatRule &stat = stats[rule->targetSlot];
    if (!petActions.changeCustomStatClamped(rule->targetSlot, rule->delta, stat.minValue, stat.maxValue))
        return false;

    char selectedAnimation[16] = {};
    if (!animations.queueActionAnimation(rule->animation,
                                         kActionDurationMs,
                                         false,
                                         AnimationOwner::Command,
                                         AnimationPriority::High,
                                         selectedAnimation,
                                         sizeof(selectedAnimation)))
        animations.showResourceError();
    else
        applyVariantEffect(selectedAnimation, petActions);
    animations.markDirty();
    return true;
}

const CustomRules::ActionRule *CustomRules::findAction(const char *key) const
{
    if (!enabled || key == nullptr || key[0] == '\0')
        return nullptr;

    for (uint8_t index = 0; index < actionRuleCount; ++index)
    {
        if (strcmp(actionRules[index].key, key) == 0)
            return &actionRules[index];
    }
    return nullptr;
}

const CustomRules::VariantEffect *CustomRules::findVariantEffect(const char *animationName) const
{
    if (!enabled || animationName == nullptr || animationName[0] == '\0')
        return nullptr;

    for (uint8_t index = 0; index < variantEffectCount; ++index)
    {
        if (strcmp(variantEffects[index].animation, animationName) == 0)
            return &variantEffects[index];
    }
    return nullptr;
}

#else

void CustomRules::clear()
{
}

bool CustomRules::load(SdFat *, DebugDisplay *)
{
    return false;
}

bool CustomRules::isEnabled() const
{
    return false;
}

bool CustomRules::hasAction(const char *) const
{
    return false;
}

void CustomRules::applyInitialValues(Pet &) const
{
}

void CustomRules::clampValues(Pet &) const
{
}

void CustomRules::applyDaily(PetActionController &) const
{
}

bool CustomRules::applyVariantEffect(const char *, PetActionController &) const
{
    return false;
}

bool CustomRules::executeAction(const char *, PetActionController &, AnimationController &) const
{
    return false;
}

#endif
