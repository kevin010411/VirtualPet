#pragma once

#include <SdFat.h>
#include "resources/detail/RuntimeTableReader.h"

namespace RuntimeTableInternal
{
// Owns the file and Source context for a load/query. The copied directory
// remains valid even if another query opens a different bundle.
class RuntimeTableFile
{
public:
    RuntimeTableFile() = default;
    ~RuntimeTableFile();
    RuntimeTableFile(const RuntimeTableFile &) = delete;
    RuntimeTableFile &operator=(const RuntimeTableFile &) = delete;

    bool open(SdFat *sd, const AssetData::RuntimeManifest &manifest);
    const RuntimeTable &table() const { return table_; }

private:
    SdBaseFile file_;
    RuntimeTable table_ = {};
};
} // namespace RuntimeTableInternal
