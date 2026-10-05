#include "shared/runtime_table/detail/RuntimeTableFile.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "shared/sd/SdBinaryRead.h"

namespace RuntimeTableInternal
{
namespace
{
constexpr char kRuntimeTablePath[] = "/runtime.bin";

bool readFile(void *context, uint32_t offset, uint8_t *destination, size_t size)
{
    auto &file = *static_cast<SdBaseFile *>(context);
    return destination != nullptr && file.seekSet(offset) &&
           readSdBinary(file, destination, size) == static_cast<int>(size);
}
} // namespace

RuntimeTableFile::~RuntimeTableFile() { file_.close(); }

bool RuntimeTableFile::open(SdFat *sd, const AssetData::RuntimeManifest &manifest)
{
    if (sd == nullptr || !file_.open(kRuntimeTablePath, FILE_READ))
        return false;
    const Source source = {&file_, readFile, file_.fileSize()};
    return readRuntimeTable(source, &manifest, table_);
}
} // namespace RuntimeTableInternal

bool loadRuntimeManifest(SdFat *sd, AssetData::RuntimeManifest &manifest)
{
    using namespace RuntimeTableInternal;
    manifest = {};
    if (sd == nullptr)
        return false;
    SdBaseFile file;
    if (!file.open(kRuntimeTablePath, FILE_READ))
        return false;
    const Source source = {&file, readFile, file.fileSize()};
    const bool decoded = readRuntimeManifest(source, manifest);
    file.close();
    return decoded;
}
