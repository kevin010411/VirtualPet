#pragma once

#include <SdFat.h>
#include "shared/runtime_table/detail/RuntimeTableReader.h"

namespace RuntimeTableInternal
{
// One fresh, validated snapshot per load/query. Owns the file and Source context;
// destroying the snapshot closes the file, including failed opens/decodes.
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
