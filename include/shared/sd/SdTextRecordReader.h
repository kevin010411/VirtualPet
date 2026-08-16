#ifndef SD_TEXT_RECORD_READER_H
#define SD_TEXT_RECORD_READER_H

#include <stddef.h>
#include <stdint.h>
#include <SdFat.h>

constexpr size_t kSdTextRecordMaxLineBytes = 128;
constexpr size_t kSdDelimitedTextMaxLineBytes = 192;
constexpr uint8_t kSdTextRecordMaxFields = 8;

struct SdTextRecord
{
    char *fields[kSdTextRecordMaxFields];
    uint8_t fieldCount;
    bool fieldOverflow;
};

using SdTextRecordHandler = bool (*)(void *context, const SdTextRecord &record);

enum class SdTextRecordAction : uint8_t
{
    Continue,
    Stop,
    Error
};

using SdDelimitedTextRecordHandler = SdTextRecordAction (*)(void *context, const SdTextRecord &record);

bool parseSdTextRecords(const char *text, size_t maxFileBytes, const char *fileIdentity,
                        const char *supportedVersion, SdTextRecordHandler handler, void *context);
bool loadSdTextRecords(SdFat *sd, const char *path, size_t maxFileBytes, const char *fileIdentity,
                       const char *supportedVersion, SdTextRecordHandler handler, void *context);
bool loadSdDelimitedTextRecords(SdFat *sd, const char *path, size_t maxFileBytes, size_t maxLineBytes,
                                SdDelimitedTextRecordHandler handler, void *context);

#endif // SD_TEXT_RECORD_READER_H
