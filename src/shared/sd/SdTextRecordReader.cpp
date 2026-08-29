#include "shared/sd/SdTextRecordReader.h"

#include <string.h>

namespace
{
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
        else if (*cursor >= 'a' && *cursor <= 'f')
            digit = static_cast<uint8_t>(*cursor - 'a' + 10);
        else
            return false;
        parsed = (parsed << 4U) | digit;
    }
    value = parsed;
    return true;
}

bool splitFields(char *line, SdTextRecord &record)
{
    record = {};
    char *cursor = line;
    while (true)
    {
        if (record.fieldCount >= kSdTextRecordMaxFields)
        {
            record.fieldOverflow = true;
            return true;
        }
        record.fields[record.fieldCount++] = cursor;
        char *separator = strchr(cursor, '|');
        if (separator == nullptr)
            return true;
        *separator = '\0';
        cursor = separator + 1;
    }
}

class RecordReader
{
public:
    RecordReader(const char *identity, const char *version, SdTextRecordHandler recordHandler, void *recordContext)
        : fileIdentity(identity), supportedVersion(version), handler(recordHandler), context(recordContext) {}

    bool process(char *line, size_t length)
    {
        if (length == 0 || length >= kSdTextRecordMaxLineBytes || finished)
            return false;
        if (line[length - 1] == '\r')
            return false;
        line[length] = '\0';
        if (length == 0)
            return false;

        char crcLine[kSdTextRecordMaxLineBytes] = {};
        strcpy(crcLine, line);
        SdTextRecord record = {};
        if (!splitFields(line, record) || record.fieldOverflow)
            return false;
        if (!headerSeen)
        {
            if (record.fieldCount != 2 || strcmp(record.fields[0], fileIdentity) != 0 ||
                strcmp(record.fields[1], supportedVersion) != 0)
                return false;
            headerSeen = true;
            crc = crc32UpdateLine(crc, crcLine);
            return true;
        }
        if (strcmp(record.fields[0], "crc32") == 0)
        {
            uint32_t expected = 0;
            if (record.fieldCount != 2 || !parseCrc32(record.fields[1], expected) ||
                (crc ^ 0xFFFFFFFFUL) != expected)
                return false;
            finished = true;
            return true;
        }
        crc = crc32UpdateLine(crc, crcLine);
        return handler != nullptr && handler(context, record);
    }

    bool complete() const { return headerSeen && finished; }

private:
    const char *fileIdentity;
    const char *supportedVersion;
    SdTextRecordHandler handler;
    void *context;
    uint32_t crc = 0xFFFFFFFFUL;
    bool headerSeen = false;
    bool finished = false;
};

template <typename NextByte>
bool readRecords(NextByte nextByte, size_t maxFileBytes, const char *fileIdentity,
                 const char *supportedVersion, SdTextRecordHandler handler, void *context)
{
    if (maxFileBytes == 0 || fileIdentity == nullptr || supportedVersion == nullptr)
        return false;
    RecordReader reader(fileIdentity, supportedVersion, handler, context);
    char line[kSdTextRecordMaxLineBytes] = {};
    size_t lineLength = 0;
    size_t totalLength = 0;
    while (true)
    {
        const int next = nextByte();
        if (next < 0)
            break;
        if (++totalLength > maxFileBytes)
            return false;
        const char character = static_cast<char>(next);
        if (character == '\n')
        {
            if (!reader.process(line, lineLength))
                return false;
            lineLength = 0;
        }
        else if (lineLength + 1 >= sizeof(line))
            return false;
        else
            line[lineLength++] = character;
    }
    if (lineLength > 0 && !reader.process(line, lineLength))
        return false;
    return reader.complete();
}

template <typename NextByte>
bool readDelimitedRecords(NextByte nextByte, size_t maxFileBytes, size_t maxLineBytes,
                          SdDelimitedTextRecordHandler handler, void *context)
{
    if (maxFileBytes == 0 || maxLineBytes < 2 || maxLineBytes > kSdDelimitedTextMaxLineBytes || handler == nullptr)
        return false;

    char line[kSdDelimitedTextMaxLineBytes] = {};
    size_t lineLength = 0;
    size_t totalLength = 0;
    while (true)
    {
        const int next = nextByte();
        if (next < 0)
            break;
        if (++totalLength > maxFileBytes)
            return false;

        const char character = static_cast<char>(next);
        if (character == '\n')
        {
            if (lineLength > 0 && line[lineLength - 1] == '\r')
                --lineLength;
            line[lineLength] = '\0';
            SdTextRecord record = {};
            if (!splitFields(line, record))
                return false;
            const SdTextRecordAction action = handler(context, record);
            if (action == SdTextRecordAction::Stop)
                return true;
            if (action == SdTextRecordAction::Error)
                return false;
            lineLength = 0;
        }
        else if (lineLength + 1 >= maxLineBytes)
            return false;
        else
            line[lineLength++] = character;
    }

    if (lineLength > 0)
    {
        if (line[lineLength - 1] == '\r')
            --lineLength;
        line[lineLength] = '\0';
        SdTextRecord record = {};
        if (!splitFields(line, record))
            return false;
        return handler(context, record) != SdTextRecordAction::Error;
    }
    return true;
}
} // namespace

bool parseSdTextRecords(const char *text, size_t maxFileBytes, const char *fileIdentity,
                        const char *supportedVersion, SdTextRecordHandler handler, void *context)
{
    if (text == nullptr)
        return false;
    const char *cursor = text;
    return readRecords([&cursor]() -> int {
        if (*cursor == '\0')
            return -1;
        return static_cast<uint8_t>(*cursor++);
    }, maxFileBytes, fileIdentity, supportedVersion, handler, context);
}

bool loadSdTextRecords(SdFat *sd, const char *path, size_t maxFileBytes, const char *fileIdentity,
                       const char *supportedVersion, SdTextRecordHandler handler, void *context)
{
    if (sd == nullptr || path == nullptr)
        return false;
    File file = sd->open(path, FILE_READ);
    if (!file)
        return false;
    const bool valid = readRecords([&file]() -> int { return file.available() ? file.read() : -1; },
                                   maxFileBytes, fileIdentity, supportedVersion, handler, context);
    file.close();
    return valid;
}

bool loadSdDelimitedTextRecords(SdFat *sd, const char *path, size_t maxFileBytes, size_t maxLineBytes,
                                SdDelimitedTextRecordHandler handler, void *context)
{
    if (sd == nullptr || path == nullptr)
        return false;
    File file = sd->open(path, FILE_READ);
    if (!file)
        return false;
    const bool valid = readDelimitedRecords([&file]() -> int { return file.available() ? file.read() : -1; },
                                            maxFileBytes, maxLineBytes, handler, context);
    file.close();
    return valid;
}
