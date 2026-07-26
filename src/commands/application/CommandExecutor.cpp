#include "commands/application/CommandExecutor.h"

#include <stdlib.h>
#include <string.h>
#include "commands/domain/StatusSetSelection.h"
#include "custom_rules/domain/CustomRules.h"

namespace
{
#if ENABLE_STATUS_SD_CONFIG
constexpr const char *kStatusDisplayPath = "/status_display.txt";
constexpr const char *kStatusSetsPath = "/status_sets.txt";
constexpr const char *kStateSchemaPath = "/state_schema.txt";
constexpr size_t kMaxStatusSources = 3;
constexpr size_t kMaxStatusSets = 3;
constexpr size_t kMaxStateAliases = 8;

struct StateAlias
{
    char name[16];
    uint8_t customIndex;
    int32_t minValue;
    int32_t maxValue;
};

struct StatusDisplayConfig
{
    AnimationId animations[kMaxStatusSources];
    char sources[kMaxStatusSources][16];
    uint8_t count;
};

struct StatusSetCondition
{
    char source[16];
    uint8_t levels;
    int32_t minValue;
    int32_t maxValue;
};

struct StatusSetConfig
{
    char animation[32];
    StatusSetCondition conditions[kMaxStatusSources];
    uint8_t conditionCount;
};

struct StatusSetsConfig
{
    StatusSetConfig sets[kMaxStatusSets];
    uint8_t count;
};

uint8_t arduinoStatusSetIndex(uint8_t setCount)
{
    return static_cast<uint8_t>(random(setCount));
}

bool isSpaceChar(char c)
{
    return c == ' ' || c == '\t';
}

char *trimField(char *text)
{
    while (text != nullptr && isSpaceChar(*text))
        ++text;

    if (text == nullptr || *text == '\0')
        return text;

    char *end = text + strlen(text) - 1;
    while (end >= text && isSpaceChar(*end))
    {
        *end = '\0';
        --end;
    }
    return text;
}

bool readConfigLine(File &file, char *line, size_t lineSize)
{
    if (line == nullptr || lineSize == 0)
        return false;

    size_t index = 0;
    bool sawAny = false;
    while (file.available())
    {
        const char c = static_cast<char>(file.read());
        sawAny = true;
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        if (index + 1 < lineSize)
            line[index++] = c;
    }

    line[index] = '\0';
    return sawAny || index > 0;
}

bool parseInt32Field(const char *text, int32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;

    char *end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0')
        return false;

    value = static_cast<int32_t>(parsed);
    return true;
}

bool parseCustomSlot(const char *text, uint8_t &index)
{
    constexpr const char *prefix = "custom";
    if (text == nullptr || strncmp(text, prefix, strlen(prefix)) != 0)
        return false;

    const char *digits = text + strlen(prefix);
    if (digits[0] < '0' || digits[0] > '7' || digits[1] != '\0')
        return false;

    index = static_cast<uint8_t>(digits[0] - '0');
    return true;
}

bool copySmallToken(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr || source[0] == '\0')
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    memcpy(dest, source, len + 1);
    return true;
}

uint8_t splitCsv(char *text, char values[][16], uint8_t maxValues)
{
    if (text == nullptr || values == nullptr || maxValues == 0)
        return 0;

    uint8_t count = 0;
    char *cursor = text;
    while (cursor != nullptr && count < maxValues)
    {
        char *comma = strchr(cursor, ',');
        if (comma != nullptr)
            *comma = '\0';

        char *token = trimField(cursor);
        if (token == nullptr || token[0] == '\0' || !copySmallToken(values[count], 16, token))
            return 0;

        ++count;
        if (comma == nullptr)
            break;
        cursor = comma + 1;
    }

    return count;
}

bool parseStatusSetCondition(char *text, StatusSetCondition &condition)
{
    char *source = strtok(text, ",");
    char *levels = strtok(nullptr, ",");
    char *minValue = strtok(nullptr, ",");
    char *maxValue = strtok(nullptr, ",");
    if (source == nullptr || levels == nullptr || minValue == nullptr || maxValue == nullptr || strtok(nullptr, ",") != nullptr)
        return false;
    int32_t parsedLevels = 0;
    if (!copySmallToken(condition.source, sizeof(condition.source), trimField(source)) ||
        !parseInt32Field(trimField(levels), parsedLevels) ||
        !parseInt32Field(trimField(minValue), condition.minValue) ||
        !parseInt32Field(trimField(maxValue), condition.maxValue) ||
        parsedLevels < 1 || parsedLevels > 32 || condition.minValue >= condition.maxValue)
        return false;
    condition.levels = static_cast<uint8_t>(parsedLevels);
    return strcmp(condition.source, "sick") != 0 || condition.levels == 2;
}

bool loadStatusSetsConfig(SdFat *sd, StatusSetsConfig &config)
{
    config = {};
    if (sd == nullptr)
        return false;
    File file = sd->open(kStatusSetsPath, FILE_READ);
    if (!file)
        return false;
    char line[192];
    bool versionSeen = false;
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;
        if (!versionSeen)
        {
            versionSeen = strcmp(content, "version=1") == 0;
            if (!versionSeen) { file.close(); return false; }
            continue;
        }
        if (config.count >= kMaxStatusSets)
        {
            file.close(); return false;
        }
        char *separator = strchr(content, '|');
        if (separator == nullptr || strchr(separator + 1, '|') != nullptr)
        {
            file.close(); return false;
        }
        *separator = '\0';
        StatusSetConfig &set = config.sets[config.count];
        if (!copySmallToken(set.animation, sizeof(set.animation), trimField(content)))
        {
            file.close(); return false;
        }
        char *descriptors = trimField(separator + 1);
        if (strcmp(set.animation, "Status") == 0)
        {
            if (descriptors[0] != '\0') { file.close(); return false; }
            ++config.count;
            continue;
        }
        char *cursor = descriptors;
        while (cursor != nullptr && set.conditionCount < kMaxStatusSources)
        {
            char *next = strchr(cursor, ';');
            if (next != nullptr) *next = '\0';
            if (!parseStatusSetCondition(trimField(cursor), set.conditions[set.conditionCount]))
            {
                file.close(); return false;
            }
            ++set.conditionCount;
            cursor = next == nullptr ? nullptr : next + 1;
        }
        if (set.conditionCount == 0 || cursor != nullptr)
        {
            file.close(); return false;
        }
        uint16_t product = 1;
        for (uint8_t i = 0; i < set.conditionCount; ++i)
        {
            product = static_cast<uint16_t>(product * set.conditions[i].levels);
            if (product > 256) { file.close(); return false; }
        }
        ++config.count;
    }
    file.close();
    return versionSeen && config.count > 0;
}

bool splitStatusDisplayRow(char *line, char *&mode, char *&animationsText, char *&sourcesText)
{
    if (line == nullptr)
        return false;

    char *firstSep = strchr(line, '|');
    if (firstSep == nullptr)
        return false;
    *firstSep = '\0';

    char *secondSep = strchr(firstSep + 1, '|');
    if (secondSep == nullptr)
        return false;
    *secondSep = '\0';

    if (strchr(secondSep + 1, '|') != nullptr)
        return false;

    mode = trimField(line);
    animationsText = trimField(firstSep + 1);
    sourcesText = trimField(secondSep + 1);
    return mode != nullptr && animationsText != nullptr && sourcesText != nullptr &&
           mode[0] != '\0' && animationsText[0] != '\0';
}

uint8_t loadStateAliases(SdFat *sd, StateAlias *aliases, uint8_t maxAliases)
{
    if (sd == nullptr || aliases == nullptr || maxAliases == 0)
        return 0;

    File file = sd->open(kStateSchemaPath, FILE_READ);
    if (!file)
        return 0;

    uint8_t count = 0;
    char line[96];
    while (count < maxAliases && readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *alias = strtok(content, "|");
        char *slot = strtok(nullptr, "|");
        char *minText = strtok(nullptr, "|");
        char *maxText = strtok(nullptr, "|");
        if (alias == nullptr || slot == nullptr || minText == nullptr || maxText == nullptr)
            continue;

        uint8_t customIndex = 0;
        int32_t minValue = 0;
        int32_t maxValue = 0;
        alias = trimField(alias);
        slot = trimField(slot);
        minText = trimField(minText);
        maxText = trimField(maxText);
        if (!parseCustomSlot(slot, customIndex) ||
            !parseInt32Field(minText, minValue) ||
            !parseInt32Field(maxText, maxValue) ||
            minValue >= maxValue ||
            !copySmallToken(aliases[count].name, sizeof(aliases[count].name), alias))
            continue;

        aliases[count].customIndex = customIndex;
        aliases[count].minValue = minValue;
        aliases[count].maxValue = maxValue;
        ++count;
    }

    file.close();
    return count;
}

bool resolveStatusSource(const char *source,
                         const PetStatSnapshot &stats,
                         const StateAlias *aliases,
                         uint8_t aliasCount,
                         int32_t &value,
                         int32_t &minValue,
                         int32_t &maxValue)
{
    if (source == nullptr)
        return false;

    minValue = 0;
    maxValue = 100;
    if (strcmp(source, "age") == 0)
        value = stats.age;
    else if (strcmp(source, "hunger") == 0)
        value = stats.hunger;
    else if (strcmp(source, "satiety") == 0)
        value = 100 - stats.hunger;
    else if (strcmp(source, "mood") == 0)
        value = stats.mood;
    else if (strcmp(source, "clean") == 0)
    {
        value = stats.clean;
        maxValue = 300;
    }
    else if (strcmp(source, "env") == 0)
    {
        value = stats.env;
        maxValue = 1000;
    }
    else if (strcmp(source, "sick") == 0)
    {
        value = stats.sick;
        maxValue = 1;
    }
    else if (strcmp(source, "status") == 0)
    {
        value = stats.status;
        maxValue = 5;
    }
    else if (strcmp(source, "stage_days") == 0)
        value = static_cast<int32_t>(stats.stage_days);
    else if (strcmp(source, "health") == 0)
    {
        value = stats.health;
        maxValue = 100;
    }
    else if (strcmp(source, "stage_progress") == 0)
    {
        value = static_cast<int32_t>(stats.stage_days);
        maxValue = 10;
    }
    else
    {
        uint8_t customIndex = 0;
        if (parseCustomSlot(source, customIndex))
        {
            value = stats.customStats[customIndex];
        }
        else
        {
            for (uint8_t i = 0; i < aliasCount; ++i)
            {
                if (strcmp(source, aliases[i].name) != 0)
                    continue;
                value = stats.customStats[aliases[i].customIndex];
                minValue = aliases[i].minValue;
                maxValue = aliases[i].maxValue;
                return true;
            }
            return false;
        }
    }

    return maxValue > minValue;
}

uint16_t frameForValue(int32_t value, int32_t minValue, int32_t maxValue, uint16_t maxFrame)
{
    if (maxFrame <= 1 || maxValue <= minValue)
        return 1;

    if (value <= minValue)
        return 1;
    if (value >= maxValue)
        return maxFrame;

    const int32_t range = maxValue - minValue;
    const int32_t offset = value - minValue;
    const uint32_t frameOffset =
        (static_cast<uint32_t>(offset) * static_cast<uint32_t>(maxFrame - 1) + static_cast<uint32_t>(range - 1)) /
        static_cast<uint32_t>(range);
    return static_cast<uint16_t>(frameOffset + 1);
}

uint8_t levelForValue(int32_t value, int32_t minValue, int32_t maxValue, uint8_t levels)
{
    if (levels <= 1 || maxValue <= minValue || value <= minValue)
        return 0;
    if (value >= maxValue)
        return static_cast<uint8_t>(levels - 1);

    const int32_t range = maxValue - minValue;
    const int32_t offset = value - minValue;
    const uint32_t level =
        (static_cast<uint32_t>(offset) * static_cast<uint32_t>(levels - 1) + static_cast<uint32_t>(range - 1)) /
        static_cast<uint32_t>(range);
    return static_cast<uint8_t>(level);
}

#if APP_STATUS_MODE == STATUS_MODE_SINGLE_METER
constexpr const char *kCompiledStatusModeName = "single_meter";
constexpr uint8_t kExpectedStatusAnimations = 1;
constexpr uint8_t kExpectedStatusSources = 1;
#elif APP_STATUS_MODE == STATUS_MODE_RANDOM_METERS
constexpr const char *kCompiledStatusModeName = "random_meters";
constexpr uint8_t kExpectedStatusAnimations = 3;
constexpr uint8_t kExpectedStatusSources = 3;
#elif APP_STATUS_MODE == STATUS_MODE_TRIPLE_METER
constexpr const char *kCompiledStatusModeName = "triple_meter";
constexpr uint8_t kExpectedStatusAnimations = 1;
constexpr uint8_t kExpectedStatusSources = 3;
#endif

bool loadStatusDisplayConfig(SdFat *sd, StatusDisplayConfig &config)
{
    config = {};
    if (sd == nullptr)
        return false;

    File file = sd->open(kStatusDisplayPath, FILE_READ);
    if (!file)
        return false;

    char line[128];
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *mode = nullptr;
        char *animationsText = nullptr;
        char *sourcesText = nullptr;
        if (!splitStatusDisplayRow(content, mode, animationsText, sourcesText))
            continue;

        if (strcmp(mode, kCompiledStatusModeName) != 0)
            continue;

        char animationTokens[kMaxStatusSources][16] = {};
        char sourceTokens[kMaxStatusSources][16] = {};
        const uint8_t animationCount = splitCsv(animationsText, animationTokens, kMaxStatusSources);
        const uint8_t sourceCount = splitCsv(sourcesText, sourceTokens, kMaxStatusSources);
        if (animationCount != kExpectedStatusAnimations || sourceCount != kExpectedStatusSources)
            continue;

        for (uint8_t i = 0; i < animationCount; ++i)
        {
            config.animations[i] = animationIdFromName(animationTokens[i]);
            if (config.animations[i] == AnimationId::None)
                return false;
        }
        for (uint8_t i = 0; i < sourceCount; ++i)
            copySmallToken(config.sources[i], sizeof(config.sources[i]), sourceTokens[i]);

        config.count = sourceCount;
        file.close();
        return true;
    }

    file.close();
    return false;
}
#endif
} // namespace

CommandExecutor::CommandExecutor(PetActionController &petActionsRef, AnimationController &animationsRef, CustomRules &customRulesRef)
    : petActions(petActionsRef),
      animations(animationsRef),
      customRules(customRulesRef)
{
}

void CommandExecutor::begin(AppCommandId commandId)
{
    currentResult = {};
    currentResult.commandId = commandId;
    currentResult.executed = true;
}

CommandResult CommandExecutor::complete(bool executed)
{
    currentResult.executed = executed && currentResult.executed;
    return currentResult;
}

#if ENABLE_COMMAND_PREDICT
AnimationId CommandExecutor::fortuneToAnimationId(int fortuneIndex)
{
    switch (fortuneIndex)
    {
    case 1:
        return AnimationId::Predict1;
    case 2:
        return AnimationId::Predict2;
    case 3:
        return AnimationId::Predict3;
    case 4:
        return AnimationId::Predict4;
    case 5:
        return AnimationId::Predict5;
    case 6:
        return AnimationId::Predict6;
    case 7:
        return AnimationId::Predict7;
    case 8:
        return AnimationId::Predict8;
    case 9:
        return AnimationId::Predict9;
    case 10:
        return AnimationId::Predict10;
    default:
        return AnimationId::Predict11;
    }
}
#endif

bool CommandExecutor::queueCommandAction(AnimationId id,
                                         unsigned long durationMs,
                                         bool playOnce,
                                         char *selectedName,
                                         size_t selectedNameSize)
{
    const bool queued = animations.queueActionAnimation(
        id, durationMs, playOnce, AnimationOwner::Command, AnimationPriority::High, selectedName, selectedNameSize);
    if (!queued)
        animations.showResourceError();
    return queued;
}

void CommandExecutor::queuePostCommandHappyAnimation()
{
    if (!animations.hasActionAnimation(AnimationId::Happy))
        return;

    animations.queueActionAnimation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High);
}

#if ENABLE_COMMAND_GIFT || ENABLE_GUESS_ITEM_GAME
void CommandExecutor::queueGiftAnimation()
{
    char selectedAnimation[16] = {};
    const bool queued = queueCommandAction(AnimationId::Gift, gameTick * 2.5, false, selectedAnimation, sizeof(selectedAnimation));
    if (!queued || !customRules.applyVariantEffect(selectedAnimation, petActions))
        petActions.changeMood(50);

    if (animations.hasActionAnimation(AnimationId::GiftHappy))
        animations.queueActionAnimation(AnimationId::GiftHappy, gameTick * 1.5, false, AnimationOwner::Command, AnimationPriority::High);

    queuePostCommandHappyAnimation();
    animations.markDirty();
}
#endif

bool CommandExecutor::commandHasAnimation(AnimationId id) const
{
    return animations.hasActionAnimation(id);
}

bool CommandExecutor::commandCanStatus() const
{
#if APP_STATUS_MODE == STATUS_MODE_DIRECT || APP_STATUS_MODE == STATUS_MODE_SINGLE_METER || APP_STATUS_MODE == STATUS_MODE_RANDOM_METERS || APP_STATUS_MODE == STATUS_MODE_COMPOSITE_HEALTH || APP_STATUS_MODE == STATUS_MODE_TRIPLE_METER
    return true;
#else
    return false;
#endif
}

#if ENABLE_CUSTOM_RULES
bool CommandExecutor::commandCanCustomAction(uint8_t slot) const
{
    if (slot > 7)
        return false;
    const char key[] = {'C', 'U', 'S', 'T', 'O', 'M', static_cast<char>('0' + slot), '\0'};
    return customRules.hasAction(key);
}
#endif

AnimationId CommandExecutor::commandCurrentAgeAnimation() const
{
    return petActions.currentAgeAnimation();
}

void CommandExecutor::commandClearCommandAnimations()
{
    animations.clearByOwner(AnimationOwner::Command);
}

void CommandExecutor::commandFeedPet()
{
    currentResult.layoutId = AnimationId::Feed;
    petActions.feedPet(40);
    queueCommandAction(AnimationId::Feed, gameTick * 1.2);
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

#if ENABLE_COMMAND_PREDICT
void CommandExecutor::commandPredict()
{
    currentResult.layoutId = AnimationId::PredAnim;
    animations.queueAnimation(Animation(AnimationId::PredAnim, gameTick * 20, true, AnimationOwner::Command, AnimationPriority::High));
    animations.queueAnimation(Animation(fortuneToAnimationId(random(1, maxFortune + 1)), gameTick * 2.4, false, AnimationOwner::Command, AnimationPriority::High));
    animations.markDirty();
}
#endif

#if ENABLE_COMMAND_GIFT
void CommandExecutor::commandGift()
{
    currentResult.layoutId = AnimationId::Gift;
    queueGiftAnimation();
}
#endif

void CommandExecutor::commandMedicine()
{
    currentResult.layoutId = AnimationId::Heal;
    petActions.takeMedicine();
    queueCommandAction(AnimationId::Heal, gameTick * 1.2);
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

void CommandExecutor::commandShower()
{
    currentResult.layoutId = AnimationId::Shower;
    petActions.takeShower(250);
    queueCommandAction(AnimationId::Shower, gameTick * 1.2);
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

#if ENABLE_GUESS_ITEM_GAME
void CommandExecutor::commandHaveFun()
{
    animations.clearByOwner(AnimationOwner::Command);
    if (canPlayGuessItemGame())
        currentResult.requestedMinigame = true;
    else
        queueGiftAnimation();
}
#endif

void CommandExecutor::commandClean()
{
    currentResult.layoutId = AnimationId::Clean;
    petActions.cleanEnvironment(500);
    queueCommandAction(AnimationId::Clean, gameTick * 1.2);
    queuePostCommandHappyAnimation();
    animations.markDirty();
}

#if ENABLE_COMMAND_OUTFIT
void CommandExecutor::commandChangeOutfit()
{
    currentResult.requestedOutfit = true;
}
#endif

#if ENABLE_COMMAND_SPECIES
void CommandExecutor::commandChangeSpecies()
{
    currentResult.requestedSpecies = true;
}
#endif

void CommandExecutor::commandStatus()
{
    currentResult.layoutId = AnimationId::Status;
    queueStatusAnimation();
}

#if ENABLE_CUSTOM_RULES
void CommandExecutor::commandCustomAction(uint8_t slot)
{
    if (slot > 7)
    {
        currentResult.executed = false;
        return;
    }
    const char key[] = {'C', 'U', 'S', 'T', 'O', 'M', static_cast<char>('0' + slot), '\0'};
    currentResult.executed = executeCustomAction(key);
}
#endif

void CommandExecutor::queueStatusAnimation()
{
#if ENABLE_STATUS_SD_CONFIG
    if (queueStatusSetsAnimation())
        return;
#endif
#if APP_STATUS_MODE == STATUS_MODE_DIRECT
    if (!queueStatusDirectAnimation())
        showStatusNotFound();
#elif APP_STATUS_MODE == STATUS_MODE_SINGLE_METER
    if (!queueStatusSingleMeterAnimation())
        showStatusNotFound();
#elif APP_STATUS_MODE == STATUS_MODE_RANDOM_METERS
    if (!queueStatusRandomMetersAnimation())
        showStatusNotFound();
#elif APP_STATUS_MODE == STATUS_MODE_COMPOSITE_HEALTH
    if (!queueCompositeStatusAnimation())
        showStatusNotFound();
#elif APP_STATUS_MODE == STATUS_MODE_TRIPLE_METER
    if (!queueStatusTripleMeterAnimation())
        showStatusNotFound();
#else
    showStatusNotFound();
#endif
}

void CommandExecutor::showStatusNotFound()
{
    animations.showStatusNotFound();
}

bool CommandExecutor::queueStatusDirectAnimation()
{
    if (!animations.hasAnimation(AnimationId::Status))
        return false;

    animations.queueAnimation(Animation(AnimationId::Status, gameTick * 10, true, AnimationOwner::Command, AnimationPriority::Normal));
    animations.markDirty();
    return true;
}

#if ENABLE_STATUS_SD_CONFIG
bool CommandExecutor::queueStatusSetsAnimation()
{
    StatusSetsConfig config = {};
    if (!loadStatusSetsConfig(animations.sdCard(), config))
        return false;

    uint8_t selectedSetIndex = 0;
    if (!selectStatusSetIndex(config.count, arduinoStatusSetIndex, selectedSetIndex))
        return false;
    const StatusSetConfig &set = config.sets[selectedSetIndex];
    if (set.conditionCount == 0)
    {
        if (!animations.hasNamedAnimation(set.animation))
            return false;
        animations.queueNamedAnimation(set.animation, gameTick * 10, true, AnimationOwner::Command, AnimationPriority::Normal);
        animations.markDirty();
        return true;
    }

    const uint16_t availableFrames = animations.frameCountForName(set.animation);
    if (availableFrames == 0)
        return false;

    const PetStatSnapshot stats = petActions.statSnapshot();
    StateAlias aliases[kMaxStateAliases] = {};
    const uint8_t aliasCount = loadStateAliases(animations.sdCard(), aliases, kMaxStateAliases);
    uint16_t frame = 0;
    uint16_t requiredFrames = 1;
    for (uint8_t i = 0; i < set.conditionCount; ++i)
    {
        const StatusSetCondition &condition = set.conditions[i];
        int32_t value = 0;
        int32_t sourceMin = 0;
        int32_t sourceMax = 0;
        if (!resolveStatusSource(condition.source, stats, aliases, aliasCount, value, sourceMin, sourceMax))
            return false;
        const uint8_t level = levelForValue(value, condition.minValue, condition.maxValue, condition.levels);
        frame = static_cast<uint16_t>(frame * condition.levels + level);
        requiredFrames = static_cast<uint16_t>(requiredFrames * condition.levels);
    }
    if (availableFrames != requiredFrames)
        return false;

    animations.queueNamedAnimation(set.animation, gameTick * 4, false, AnimationOwner::Command,
                                   AnimationPriority::Normal, static_cast<uint16_t>(frame + 1));
    animations.markDirty();
    return true;
}

bool CommandExecutor::queueStatusSingleMeterAnimation()
{
#if APP_STATUS_MODE == STATUS_MODE_SINGLE_METER
    StatusDisplayConfig config = {};
    if (!loadStatusDisplayConfig(animations.sdCard(), config))
        return false;

    const uint16_t maxFrame = animations.frameCountFor(config.animations[0]);
    if (maxFrame == 0)
        return false;

    const PetStatSnapshot stats = petActions.statSnapshot();
    StateAlias aliases[kMaxStateAliases] = {};
    const uint8_t aliasCount = loadStateAliases(animations.sdCard(), aliases, kMaxStateAliases);
    int32_t value = 0;
    int32_t minValue = 0;
    int32_t maxValue = 0;
    if (!resolveStatusSource(config.sources[0], stats, aliases, aliasCount, value, minValue, maxValue))
        return false;

    animations.queueAnimation(Animation(config.animations[0], gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, frameForValue(value, minValue, maxValue, maxFrame)));
    animations.markDirty();
    return true;
#else
    return false;
#endif
}

bool CommandExecutor::queueStatusRandomMetersAnimation()
{
#if APP_STATUS_MODE == STATUS_MODE_RANDOM_METERS
    StatusDisplayConfig config = {};
    if (!loadStatusDisplayConfig(animations.sdCard(), config) || config.count == 0)
        return false;

    const PetStatSnapshot stats = petActions.statSnapshot();
    StateAlias aliases[kMaxStateAliases] = {};
    const uint8_t aliasCount = loadStateAliases(animations.sdCard(), aliases, kMaxStateAliases);
    const uint8_t start = random(config.count);
    for (uint8_t attempts = 0; attempts < config.count; ++attempts)
    {
        const uint8_t index = (start + attempts) % config.count;
        const AnimationId chosen = config.animations[index];
        if (!animations.hasAnimation(chosen))
            continue;

        const uint16_t maxFrame = animations.frameCountFor(chosen);
        if (maxFrame == 0)
            continue;

        int32_t value = 0;
        int32_t minValue = 0;
        int32_t maxValue = 0;
        if (!resolveStatusSource(config.sources[index], stats, aliases, aliasCount, value, minValue, maxValue))
            continue;

        const uint16_t frame = frameForValue(value, minValue, maxValue, maxFrame);
        animations.queueAnimation(Animation(chosen, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, frame));
        animations.markDirty();
        return true;
    }

    return false;
#else
    return false;
#endif
}

bool CommandExecutor::queueStatusTripleMeterAnimation()
{
#if APP_STATUS_MODE == STATUS_MODE_TRIPLE_METER
    StatusDisplayConfig config = {};

    if (!loadStatusDisplayConfig(animations.sdCard(), config))
        return false;

    const uint16_t maxFrame = animations.frameCountFor(config.animations[0]);
    if (maxFrame < 64)
        return false;

    const PetStatSnapshot stats = petActions.statSnapshot();
    StateAlias aliases[kMaxStateAliases] = {};
    const uint8_t aliasCount = loadStateAliases(animations.sdCard(), aliases, kMaxStateAliases);
    uint8_t levels[3] = {};
    for (uint8_t i = 0; i < 3; ++i)
    {
        int32_t value = 0;
        int32_t minValue = 0;
        int32_t maxValue = 0;
        if (!resolveStatusSource(config.sources[i], stats, aliases, aliasCount, value, minValue, maxValue))
            return false;
        levels[i] = levelForValue(value, minValue, maxValue, 4);
    }

    const uint16_t frame = static_cast<uint16_t>(levels[0] * 16 + levels[1] * 4 + levels[2] + 1);
    animations.queueAnimation(Animation(config.animations[0], gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, frame));
    animations.markDirty();
    return true;
#else
    return false;
#endif
}
#endif

#if ENABLE_STATUS_COMPOSITE
bool CommandExecutor::queueCompositeStatusAnimation()
{
    const AnimationId statusAnimation = compositeStatusAnimationId();
    if (animations.frameCountFor(statusAnimation) == 0)
    {
        animations.showResourceError();
        return false;
    }

    animations.queueAnimation(Animation(statusAnimation, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal));
    animations.markDirty();
    return true;
}

AnimationId CommandExecutor::compositeStatusAnimationId() const
{
    HealthStatus bodyStatus = petActions.currentStatus();
    if (bodyStatus == HealthStatus::Depressed)
        bodyStatus = HealthStatus::Healthy;

    const bool depressed = petActions.isMoodDepressed();
    switch (bodyStatus)
    {
    case HealthStatus::Healthy:
        return depressed ? AnimationId::StatusDepressedHealthy : AnimationId::StatusGoodHealthy;
    case HealthStatus::Sick:
        return depressed ? AnimationId::StatusDepressedSick : AnimationId::StatusGoodSick;
    case HealthStatus::Hungry:
        return depressed ? AnimationId::StatusDepressedHungry : AnimationId::StatusGoodHungry;
    case HealthStatus::Poop:
        return depressed ? AnimationId::StatusDepressedPoop : AnimationId::StatusGoodPoop;
    case HealthStatus::Dirty:
        return depressed ? AnimationId::StatusDepressedDirty : AnimationId::StatusGoodDirty;
    case HealthStatus::Depressed:
        break;
    }

    return depressed ? AnimationId::StatusDepressedHealthy : AnimationId::StatusGoodHealthy;
}
#endif

#if ENABLE_GUESS_ITEM_GAME
bool CommandExecutor::canPlayGuessItemGame() const
{
    const AnimationId requiredResults[] = {
#if ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
        AnimationId::GuessLL,
        AnimationId::GuessRR,
        AnimationId::GuessWin,
        AnimationId::GuessLoss};
#else
        AnimationId::GuessLL,
        AnimationId::GuessLR,
        AnimationId::GuessRL,
        AnimationId::GuessRR};
#endif
    if (!animations.hasAnimations(requiredResults, sizeof(requiredResults) / sizeof(requiredResults[0])))
        return false;

    const AnimationId itemPrompts[] = {
        AnimationId::GuessItem1,
        AnimationId::GuessItem2,
        AnimationId::GuessItem3,
        AnimationId::GuessItem4};
    return animations.hasAnimation(AnimationId::GuessStart) ||
           animations.hasAnimations(itemPrompts, sizeof(itemPrompts) / sizeof(itemPrompts[0]));
}
#endif

#if ENABLE_CUSTOM_RULES
bool CommandExecutor::executeCustomAction(const char *actionKey)
{
    return customRules.executeAction(actionKey, petActions, animations);
}
#endif
