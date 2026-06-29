#include "storage/SdAppearanceLoader.h"

#include <stdlib.h>
#include <string.h>

namespace
{
constexpr const char *kSpeciesByHealthyDaysPath = "/species_by_healthy_days.txt";
constexpr uint32_t kNoMaxHealthyDays = 0xFFFFFFFFUL;

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

bool parseUnsignedField(const char *text, uint32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;

    uint32_t parsed = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        parsed = parsed * 10UL + static_cast<uint32_t>(*cursor - '0');
    }

    value = parsed;
    return true;
}

bool isValidAppearanceCode(const char *code)
{
    if (code == nullptr || code[0] == '\0')
        return false;

    const size_t len = strlen(code);
    if (len > 8)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = code[i];
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    return true;
}

bool splitSpeciesRule(char *line, uint32_t &minDays, uint32_t &maxDays, char *&speciesCode, char *&outfitCode)
{
    char *fields[4] = {};
    fields[0] = line;
    int fieldIndex = 1;

    for (char *cursor = line; *cursor != '\0'; ++cursor)
    {
        if (*cursor != '|')
            continue;
        if (fieldIndex >= 4)
            return false;
        *cursor = '\0';
        fields[fieldIndex++] = cursor + 1;
    }

    if (fieldIndex != 4)
        return false;

    for (int i = 0; i < 4; ++i)
        fields[i] = trimField(fields[i]);

    if (!parseUnsignedField(fields[0], minDays))
        return false;

    if (strcmp(fields[1], "*") == 0)
        maxDays = kNoMaxHealthyDays;
    else if (!parseUnsignedField(fields[1], maxDays))
        return false;

    speciesCode = fields[2];
    outfitCode = fields[3];
    return minDays <= maxDays &&
           isValidAppearanceCode(speciesCode) &&
           isValidAppearanceCode(outfitCode);
}

bool buildSpeciesOutfitListPath(char *dest, size_t destSize, const char *speciesCode)
{
    if (dest == nullptr || destSize == 0 || !isValidAppearanceCode(speciesCode))
        return false;

    const int written = snprintf(dest, destSize, "/index/%s.txt", speciesCode);
    return written >= 0 && written < static_cast<int>(destSize);
}

bool buildOutfitPreviewPath(char *dest, size_t destSize, const char *speciesCode)
{
    if (dest == nullptr || destSize == 0 || !isValidAppearanceCode(speciesCode))
        return false;

    const int written = snprintf(dest, destSize, "/index/%s_outfit.txt", speciesCode);
    return written >= 0 && written < static_cast<int>(destSize);
}

bool copyText(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr)
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    memcpy(dest, source, len + 1);
    return true;
}

bool splitOutfitPreviewRow(char *line, char *fields[], size_t fieldCount)
{
    size_t count = 0;
    char *cursor = line;

    while (count < fieldCount)
    {
        fields[count++] = cursor;
        char *sep = strchr(cursor, '|');
        if (sep == nullptr)
            break;

        *sep = '\0';
        cursor = sep + 1;
    }

    if (count != fieldCount)
        return false;

    if (strchr(fields[fieldCount - 1], '|') != nullptr)
        return false;

    for (size_t i = 0; i < fieldCount; ++i)
    {
        fields[i] = trimField(fields[i]);
        if (fields[i] == nullptr || fields[i][0] == '\0')
            return false;
    }

    return true;
}

} // namespace

SdAppearanceLoader::SdAppearanceLoader(SdFat *refSd) : sd(refSd)
{
}

bool SdAppearanceLoader::findForHealthyDays(uint32_t healthyDays, AppearanceSelection &selection)
{
    if (sd == nullptr || !sd->exists(kSpeciesByHealthyDaysPath))
        return false;

    File file = sd->open(kSpeciesByHealthyDaysPath, FILE_READ);
    if (!file)
        return false;

    char line[80] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        uint32_t minDays = 0;
        uint32_t maxDays = 0;
        char *speciesCode = nullptr;
        char *outfitCode = nullptr;
        if (!splitSpeciesRule(content, minDays, maxDays, speciesCode, outfitCode))
            continue;

        if (healthyDays < minDays || healthyDays > maxDays)
            continue;

        strncpy(selection.speciesCode, speciesCode, sizeof(selection.speciesCode) - 1);
        selection.speciesCode[sizeof(selection.speciesCode) - 1] = '\0';
        strncpy(selection.outfitCode, outfitCode, sizeof(selection.outfitCode) - 1);
        selection.outfitCode[sizeof(selection.outfitCode) - 1] = '\0';
        file.close();
        return true;
    }

    file.close();
    return false;
}

bool SdAppearanceLoader::loadSpecies(char species[][9], size_t maxSpecies, size_t &speciesCount)
{
    speciesCount = 0;
    if (sd == nullptr || species == nullptr || maxSpecies == 0 || !sd->exists(kSpeciesByHealthyDaysPath))
        return false;

    File file = sd->open(kSpeciesByHealthyDaysPath, FILE_READ);
    if (!file)
        return false;

    char line[80] = {};
    while (readConfigLine(file, line, sizeof(line)) && speciesCount < maxSpecies)
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        uint32_t minDays = 0;
        uint32_t maxDays = 0;
        char *speciesCode = nullptr;
        char *outfitCode = nullptr;
        if (!splitSpeciesRule(content, minDays, maxDays, speciesCode, outfitCode))
            continue;

        bool duplicate = false;
        for (size_t i = 0; i < speciesCount; ++i)
        {
            if (strcmp(species[i], speciesCode) == 0)
            {
                duplicate = true;
                break;
            }
        }

        if (duplicate)
            continue;

        strncpy(species[speciesCount], speciesCode, 8);
        species[speciesCount][8] = '\0';
        ++speciesCount;
    }

    file.close();
    return speciesCount > 0;
}

bool SdAppearanceLoader::loadOutfits(const char *speciesCode, char outfits[][9], size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (sd == nullptr || outfits == nullptr || maxOutfits == 0)
        return false;

    char path[32] = {};
    if (!buildSpeciesOutfitListPath(path, sizeof(path), speciesCode))
        return false;

    File file = sd->open(path, FILE_READ);
    if (!file)
        return false;

    char line[128] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *cursor = content;
        while (cursor != nullptr && *cursor != '\0' && outfitCount < maxOutfits)
        {
            char *sep = strchr(cursor, '|');
            if (sep != nullptr)
                *sep = '\0';

            char *outfitCode = trimField(cursor);
            if (isValidAppearanceCode(outfitCode))
            {
                strncpy(outfits[outfitCount], outfitCode, 8);
                outfits[outfitCount][8] = '\0';
                ++outfitCount;
            }

            cursor = (sep == nullptr) ? nullptr : sep + 1;
        }

        if (outfitCount > 0 || outfitCount >= maxOutfits)
            break;
    }

    file.close();
    return outfitCount > 0;
}

bool SdAppearanceLoader::findOutfitPreview(const char *speciesCode, const char *outfitCode, OutfitPreview &preview)
{
    preview = {};
    if (sd == nullptr || !isValidAppearanceCode(outfitCode))
        return false;

    char path[40] = {};
    if (!buildOutfitPreviewPath(path, sizeof(path), speciesCode))
        return false;

    File file = sd->open(path, FILE_READ);
    if (!file)
        return false;

    char line[192] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *fields[6] = {};
        if (!splitOutfitPreviewRow(content, fields, 6))
            continue;

        if (strcmp(fields[0], outfitCode) != 0)
            continue;

        const uint16_t frameCount = static_cast<uint16_t>(strtoul(fields[1], nullptr, 10));
        const uint16_t frameIntervalMs = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
        const uint16_t width = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
        const uint16_t height = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
        if (frameCount == 0 || width == 0 || height == 0)
            continue;

        if (!copyText(preview.outfitCode, sizeof(preview.outfitCode), fields[0]) ||
            !copyText(preview.path, sizeof(preview.path), fields[5]))
            continue;

        preview.frameCount = frameCount;
        preview.width = width;
        preview.height = height;
        preview.frameIntervalMs = frameIntervalMs;
        file.close();
        return true;
    }

    file.close();
    return false;
}
