#include "pet_behavior/domain/PetBehaviorContract.h"

#include <string.h>
#include <SdFat.h>
#include "commands/domain/SystemCommandCatalog.h"
#include "shared/config/AppProfile.h"

namespace
{
constexpr const char *kPetBehaviorPath = "/pet_behavior.txt";
constexpr size_t kMaxPetBehaviorLineBytes = 128;

uint32_t crc32UpdateByte(uint32_t crc, uint8_t value)
{
    crc ^= value;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320UL : crc >> 1U;
    return crc;
}

uint32_t crc32UpdateLine(uint32_t crc, const char *line)
{
    for (const char *cursor = line; *cursor != '\0'; ++cursor)
        crc = crc32UpdateByte(crc, static_cast<uint8_t>(*cursor));
    return crc32UpdateByte(crc, static_cast<uint8_t>('\n'));
}

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

bool parseSigned16(const char *text, int16_t &value)
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
    const uint32_t maximum = negative ? 32768U : 32767U;
    for (; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
        if (magnitude > (maximum - digit) / 10U)
            return false;
        magnitude = magnitude * 10U + digit;
    }

    value = negative ? static_cast<int16_t>(-static_cast<int32_t>(magnitude))
                     : static_cast<int16_t>(magnitude);
    return true;
}

bool parseCrc32(const char *text, uint32_t &value)
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

bool parseSlot(const char *token, const char *prefix, uint8_t &slot)
{
    if (token == nullptr || prefix == nullptr)
        return false;
    const size_t prefixLength = strlen(prefix);
    if (strncmp(token, prefix, prefixLength) != 0 || token[prefixLength] < '0' ||
        token[prefixLength] > '7' || token[prefixLength + 1] != '\0')
        return false;
    slot = static_cast<uint8_t>(token[prefixLength] - '0');
    return true;
}

bool parseAnimationToken(const char *token, char *destination, size_t destinationSize)
{
    if (token == nullptr || destination == nullptr || destinationSize == 0 || strncmp(token, "anim", 4) != 0)
        return false;
    const size_t length = strlen(token);
    if (length != 5 && length != 6)
        return false;
    uint32_t animation = 0;
    if (!parseUnsigned(token + 4, 31, animation) ||
        (length == 5 && animation > 9) ||
        (length == 6 && animation < 10) ||
        length >= destinationSize)
        return false;
    strcpy(destination, token);
    return true;
}

bool parseStatName(const char *value, char *destination, size_t destinationSize)
{
    if (value == nullptr || destination == nullptr || destinationSize == 0)
        return false;
    const size_t length = strlen(value);
    if (length == 0 || length >= destinationSize || value[0] < 'a' || value[0] > 'z')
        return false;
    for (size_t index = 1; index < length; ++index)
    {
        const char character = value[index];
        if (!((character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') || character == '_'))
            return false;
    }
    constexpr const char *reserved[] = {
        "predict", "guess_game", "status", "change_outfit", "change_species",
        "stage_days", "species", "outfit",
    };
    for (const char *name : reserved)
    {
        if (strcmp(value, name) == 0)
            return false;
    }
    strcpy(destination, value);
    return true;
}

uint8_t splitFields(char *line, char **fields, uint8_t capacity)
{
    if (line == nullptr || fields == nullptr || capacity == 0)
        return 0;

    uint8_t count = 0;
    char *cursor = line;
    while (true)
    {
        if (count >= capacity)
            return 0;
        fields[count++] = cursor;
        char *separator = strchr(cursor, '|');
        if (separator == nullptr)
            return count;
        *separator = '\0';
        cursor = separator + 1;
    }
}

class PetBehaviorParser
{
public:
    bool process(char *line)
    {
        if (line == nullptr || line[0] == '\0' || hasInvalidCharacter(line))
            return false;
        if (finished)
            return false;

        if (strncmp(line, "crc32|", 6) == 0)
            return processCrc(line);

        if (seenFooter)
            return false;
        crc = crc32UpdateLine(crc, line);

        char *fields[8] = {};
        const uint8_t fieldCount = splitFields(line, fields, sizeof(fields) / sizeof(fields[0]));
        if (fieldCount == 0)
            return false;

        switch (phase)
        {
        case Phase::Header:
            return parseHeader(fields, fieldCount);
        case Phase::Stats:
            return parseStat(fields, fieldCount);
        case Phase::Idle:
            return parseIdle(fields, fieldCount);
        case Phase::IdleTriggers:
            return parseIdleTrigger(fields, fieldCount);
        case Phase::Actions:
            return parseAction(fields, fieldCount);
        case Phase::ActionEffects:
            return parseActionEffect(fields, fieldCount);
        case Phase::Buttons:
            return parseButton(fields, fieldCount);
        default:
            return false;
        }
    }

    bool complete(PetBehaviorConfig &destination) const
    {
        if (!finished || !seenFooter || phase != Phase::Buttons ||
            statsSeen != candidate.statCount || idleTriggersSeen != candidate.idleTriggerCount ||
            actionsSeen != candidate.actionCount || actionEffectsSeen != candidate.actionEffectCount ||
            buttonsSeen != kPetBehaviorButtonCount)
            return false;
        destination = candidate;
        return true;
    }

private:
    enum class Phase : uint8_t
    {
        Header,
        Stats,
        Idle,
        IdleTriggers,
        Actions,
        ActionEffects,
        Buttons,
    };

    PetBehaviorConfig candidate = {};
    Phase phase = Phase::Header;
    uint32_t crc = 0xFFFFFFFFUL;
    uint8_t statsSeen = 0;
    uint8_t idleTriggersSeen = 0;
    uint8_t actionsSeen = 0;
    uint8_t actionEffectsSeen = 0;
    uint8_t buttonsSeen = 0;
    bool idleSeen = false;
    bool seenFooter = false;
    bool finished = false;

    static bool hasInvalidCharacter(const char *line)
    {
        for (const char *cursor = line; *cursor != '\0'; ++cursor)
        {
            if (*cursor == '\r' || *cursor == '\n')
                return true;
        }
        return false;
    }

    bool findStatSlot(const char *name, uint8_t &slot) const
    {
        for (uint8_t index = 0; index < candidate.statCount; ++index)
        {
            if (candidate.stats[index].active && strcmp(candidate.stats[index].name, name) == 0)
            {
                slot = index;
                return true;
            }
        }
        return false;
    }

    bool parseHeader(char **fields, uint8_t count)
    {
        uint32_t schemaFingerprint = 0;
        uint32_t statCount = 0;
        uint32_t idleTriggerCount = 0;
        uint32_t actionCount = 0;
        uint32_t actionEffectCount = 0;
        uint32_t buttonCount = 0;
        if (count != 8 || strcmp(fields[0], "pet_behavior") != 0 || strcmp(fields[1], "3") != 0 ||
            !parseCrc32(fields[2], schemaFingerprint) ||
            !parseUnsigned(fields[3], kMaxPetBehaviorStats, statCount) ||
            !parseUnsigned(fields[4], kMaxPetBehaviorIdleTriggers, idleTriggerCount) ||
            !parseUnsigned(fields[5], kMaxPetBehaviorActions, actionCount) ||
            !parseUnsigned(fields[6], kMaxPetBehaviorActionEffects, actionEffectCount) ||
            !parseUnsigned(fields[7], kPetBehaviorButtonCount, buttonCount) ||
            buttonCount != kPetBehaviorButtonCount)
            return false;

        candidate.schemaFingerprint = schemaFingerprint;
        candidate.statCount = static_cast<uint8_t>(statCount);
        candidate.idleTriggerCount = static_cast<uint8_t>(idleTriggerCount);
        candidate.actionCount = static_cast<uint8_t>(actionCount);
        candidate.actionEffectCount = static_cast<uint8_t>(actionEffectCount);
        candidate.buttonCount = static_cast<uint8_t>(buttonCount);
        phase = candidate.statCount == 0 ? Phase::Idle : Phase::Stats;
        return true;
    }

    bool parseStat(char **fields, uint8_t count)
    {
        int16_t initialValue = 0;
        int16_t minValue = 0;
        int16_t maxValue = 0;
        int16_t dailyChange = 0;
        if (count != 6 || strcmp(fields[0], "stat") != 0 || !parseSigned16(fields[2], initialValue) ||
            !parseSigned16(fields[3], minValue) || !parseSigned16(fields[4], maxValue) ||
            !parseSigned16(fields[5], dailyChange) || minValue >= maxValue ||
            initialValue < minValue || initialValue > maxValue)
            return false;

        for (uint8_t index = 0; index < statsSeen; ++index)
        {
            if (strcmp(candidate.stats[index].name, fields[1]) == 0)
                return false;
        }
        PetBehaviorStatConfig &stat = candidate.stats[statsSeen];
        if (!parseStatName(fields[1], stat.name, sizeof(stat.name)))
            return false;
        stat.active = true;
        stat.initialValue = initialValue;
        stat.minValue = minValue;
        stat.maxValue = maxValue;
        stat.dailyChange = dailyChange;
        ++statsSeen;
        if (statsSeen == candidate.statCount)
            phase = Phase::Idle;
        return statsSeen <= candidate.statCount;
    }

    bool parseIdle(char **fields, uint8_t count)
    {
        if (count != 2 || strcmp(fields[0], "idle") != 0 || idleSeen ||
            !parseAnimationToken(fields[1], candidate.idleAnimation, sizeof(candidate.idleAnimation)))
            return false;
        idleSeen = true;
        phase = candidate.idleTriggerCount == 0 ?
                    (candidate.actionCount == 0 ?
                         (candidate.actionEffectCount == 0 ? Phase::Buttons : Phase::ActionEffects) :
                         Phase::Actions) :
                    Phase::IdleTriggers;
        return true;
    }

    bool parseIdleTrigger(char **fields, uint8_t count)
    {
        uint8_t statSlot = 0;
        int16_t threshold = 0;
        PetBehaviorIdleTriggerOperator comparison;
        if (count != 5 || strcmp(fields[0], "idle_trigger") != 0)
            return false;
        if (!findStatSlot(fields[1], statSlot))
            return false;
        if (strcmp(fields[2], "<") == 0)
            comparison = PetBehaviorIdleTriggerOperator::LessThan;
        else if (strcmp(fields[2], ">") == 0)
            comparison = PetBehaviorIdleTriggerOperator::GreaterThan;
        else
            return false;
        const PetBehaviorStatConfig &stat = candidate.stats[statSlot];
        if (!parseSigned16(fields[3], threshold) ||
            (comparison == PetBehaviorIdleTriggerOperator::LessThan &&
             (threshold <= stat.minValue || threshold > stat.maxValue)) ||
            (comparison == PetBehaviorIdleTriggerOperator::GreaterThan &&
             (threshold < stat.minValue || threshold >= stat.maxValue)) ||
            idleTriggersSeen >= candidate.idleTriggerCount)
            return false;

        PetBehaviorIdleTriggerConfig &trigger = candidate.idleTriggers[idleTriggersSeen];
        if (!parseAnimationToken(fields[4], trigger.animation, sizeof(trigger.animation)))
            return false;
        trigger.active = true;
        trigger.statSlot = statSlot;
        trigger.comparison = comparison;
        trigger.threshold = threshold;
        ++idleTriggersSeen;
        if (idleTriggersSeen == candidate.idleTriggerCount)
            phase = candidate.actionCount == 0 ?
                        (candidate.actionEffectCount == 0 ? Phase::Buttons : Phase::ActionEffects) :
                        Phase::Actions;
        return true;
    }

    bool parseAction(char **fields, uint8_t count)
    {
        uint8_t actionSlot = 0;
        uint32_t playbackCount = 0;
        if (count != 4 || strcmp(fields[0], "action") != 0 || !parseSlot(fields[1], "action", actionSlot) ||
            candidate.actions[actionSlot].active || !parseUnsigned(fields[3], 5, playbackCount) ||
            playbackCount < 1 || actionsSeen >= candidate.actionCount)
            return false;

        PetBehaviorActionConfig &action = candidate.actions[actionSlot];
        if (!parseAnimationToken(fields[2], action.animation, sizeof(action.animation)))
            return false;
        action.active = true;
        action.playbackCount = static_cast<uint8_t>(playbackCount);
        ++actionsSeen;
        if (actionsSeen == candidate.actionCount)
            phase = candidate.actionEffectCount == 0 ? Phase::Buttons : Phase::ActionEffects;
        return true;
    }

    bool parseActionEffect(char **fields, uint8_t count)
    {
        uint8_t actionSlot = 0;
        uint8_t statSlot = 0;
        int16_t value = 0;
        PetBehaviorActionEffectConfig::Operation operation;
        if (count != 5 || strcmp(fields[0], "action_effect") != 0 ||
            !parseSlot(fields[1], "action", actionSlot) || !candidate.actions[actionSlot].active ||
            !parseSigned16(fields[4], value) || actionEffectsSeen >= candidate.actionEffectCount)
            return false;
        if (strcmp(fields[2], "change") == 0)
            operation = PetBehaviorActionEffectConfig::Operation::Change;
        else if (strcmp(fields[2], "set") == 0)
            operation = PetBehaviorActionEffectConfig::Operation::Set;
        else
            return false;
        if (!findStatSlot(fields[3], statSlot))
            return false;

        uint8_t actionEffectCount = 0;
        for (uint8_t index = 0; index < actionEffectsSeen; ++index)
        {
            const PetBehaviorActionEffectConfig &effect = candidate.actionEffects[index];
            if (effect.actionSlot == actionSlot && effect.statSlot == statSlot)
                return false;
            if (effect.actionSlot == actionSlot)
                ++actionEffectCount;
        }
        if (actionEffectCount >= kMaxPetBehaviorStats)
            return false;

        PetBehaviorActionEffectConfig &effect = candidate.actionEffects[actionEffectsSeen];
        effect.active = true;
        effect.actionSlot = actionSlot;
        effect.statSlot = statSlot;
        effect.operation = operation;
        effect.value = value;
        ++actionEffectsSeen;
        if (actionEffectsSeen == candidate.actionEffectCount)
            phase = Phase::Buttons;
        return true;
    }

    bool parseButton(char **fields, uint8_t count)
    {
        uint32_t position = 0;
        if (count != 4 || strcmp(fields[0], "button") != 0 ||
            !parseUnsigned(fields[1], kPetBehaviorButtonCount, position) || position < 1 ||
            candidate.buttons[position - 1].active || buttonsSeen >= kPetBehaviorButtonCount)
            return false;

        PetBehaviorButtonConfig &button = candidate.buttons[position - 1];
        if (strcmp(fields[2], "empty") == 0)
        {
            if (fields[3][0] != '\0')
                return false;
            button.kind = PetBehaviorButtonKind::Empty;
        }
        else if (strcmp(fields[2], "user_action") == 0)
        {
            uint8_t actionSlot = 0;
            if (!parseSlot(fields[3], "action", actionSlot) || !candidate.actions[actionSlot].active)
                return false;
            button.kind = PetBehaviorButtonKind::UserAction;
            button.actionSlot = actionSlot;
        }
        else if (strcmp(fields[2], "system_command") == 0)
        {
            if (findCompiledSystemCommand(fields[3]) == nullptr ||
                strlen(fields[3]) >= sizeof(button.systemCommand))
                return false;
            button.kind = PetBehaviorButtonKind::SystemCommand;
            strcpy(button.systemCommand, fields[3]);
        }
        else
        {
            return false;
        }

        button.active = true;
        ++buttonsSeen;
        return true;
    }

    bool processCrc(char *line)
    {
        char *fields[3] = {};
        const uint8_t count = splitFields(line, fields, sizeof(fields) / sizeof(fields[0]));
        uint32_t expected = 0;
        if (count != 2 || strcmp(fields[0], "crc32") != 0 || !parseCrc32(fields[1], expected) ||
            phase != Phase::Buttons || !idleSeen || statsSeen != candidate.statCount ||
            idleTriggersSeen != candidate.idleTriggerCount || actionsSeen != candidate.actionCount ||
            actionEffectsSeen != candidate.actionEffectCount || buttonsSeen != kPetBehaviorButtonCount ||
            (crc ^ 0xFFFFFFFFUL) != expected)
            return false;
        seenFooter = true;
        finished = true;
        return true;
    }
};

bool processTextLine(PetBehaviorParser &parser, char *line, size_t length)
{
    if (length >= kMaxPetBehaviorLineBytes)
        return false;
    if (length > 0 && line[length - 1] == '\r')
        line[--length] = '\0';
    else
        line[length] = '\0';
    return parser.process(line);
}
} // namespace

bool parsePetBehaviorContract(const char *contractText, PetBehaviorConfig &config)
{
    if (contractText == nullptr)
        return false;

    PetBehaviorParser parser;
    char line[kMaxPetBehaviorLineBytes] = {};
    size_t lineLength = 0;
    size_t totalLength = 0;
    for (const char *cursor = contractText; *cursor != '\0'; ++cursor)
    {
        if (++totalLength >= kMaxPetBehaviorContractBytes || lineLength + 1 >= sizeof(line))
            return false;
        if (*cursor == '\n')
        {
            line[lineLength] = '\0';
            if (!processTextLine(parser, line, lineLength))
                return false;
            lineLength = 0;
            continue;
        }
        line[lineLength++] = *cursor;
    }

    if (lineLength > 0 && !processTextLine(parser, line, lineLength))
        return false;
    return parser.complete(config);
}

bool loadPetBehaviorContract(SdFat *sd, PetBehaviorConfig &config)
{
    if (sd == nullptr)
        return false;
    File file = sd->open(kPetBehaviorPath, FILE_READ);
    if (!file)
        return false;

    PetBehaviorParser parser;
    char line[kMaxPetBehaviorLineBytes] = {};
    size_t lineLength = 0;
    size_t totalLength = 0;
    bool valid = true;
    while (file.available())
    {
        const int next = file.read();
        if (next < 0 || ++totalLength >= kMaxPetBehaviorContractBytes)
        {
            valid = false;
            break;
        }
        const char character = static_cast<char>(next);
        if (character == '\n')
        {
            line[lineLength] = '\0';
            if (!processTextLine(parser, line, lineLength))
            {
                valid = false;
                break;
            }
            lineLength = 0;
        }
        else if (lineLength + 1 >= sizeof(line))
        {
            valid = false;
            break;
        }
        else
        {
            line[lineLength++] = character;
        }
    }
    file.close();

    if (!valid || (lineLength > 0 && !processTextLine(parser, line, lineLength)))
        return false;
    return parser.complete(config);
}
