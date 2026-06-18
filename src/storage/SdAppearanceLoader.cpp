#include "storage/SdAppearanceLoader.h"

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
