#include "resources/detail/RuntimeTableFile.h"
#include "resources/RuntimeTableBehavior.h"
#include "common/SdBinaryRead.h"

namespace RuntimeTableInternal
{
namespace
{
constexpr char kRuntimeTablePath[] = "/runtime.bin";

struct DirectoryCache
{
    SdFat *card = nullptr;
    AssetData::RuntimeManifest manifest = {};
    RuntimeTableDirectory directory = {};

    bool matches(SdFat *sd, const AssetData::RuntimeManifest &candidate) const
    {
        return card == sd && manifest.fileSize == candidate.fileSize &&
               manifest.fileCrc32 == candidate.fileCrc32 &&
               manifest.schemaFingerprint == candidate.schemaFingerprint &&
               AssetData::sameBundleId(manifest.bundleId, candidate.bundleId);
    }
};

DirectoryCache cache;
static_assert(sizeof(DirectoryCache) <= 576, "Runtime directory cache RAM budget");

bool readFile(void *context, uint32_t offset, uint8_t *destination, size_t size)
{
    auto &file = *static_cast<SdBaseFile *>(context);
    if (destination != nullptr && file.seekSet(offset) &&
        readSdBinary(file, destination, size) == static_cast<int>(size))
        return true;
    cache.card = nullptr;
    return false;
}
} // namespace

RuntimeTableFile::~RuntimeTableFile() { file_.close(); }

bool RuntimeTableFile::open(SdFat *sd, const AssetData::RuntimeManifest &manifest)
{
    if (sd == nullptr || !file_.open(kRuntimeTablePath, FILE_READ))
    {
        cache.card = nullptr;
        return false;
    }
    const Source source = {&file_, readFile, file_.fileSize()};
    const RuntimeTableDirectory *directory = cache.matches(sd, manifest) ? &cache.directory : nullptr;
    if (!readRuntimeTable(source, &manifest, table_, directory))
    {
        cache.card = nullptr;
        return false;
    }
    cache.manifest = manifest;
    cache.directory = table_;
    cache.card = sd;
    return true;
}
} // namespace RuntimeTableInternal

bool loadRuntimeManifest(SdFat *sd, AssetData::RuntimeManifest &manifest)
{
    using namespace RuntimeTableInternal;
    manifest = {};
    if (sd == nullptr)
    {
        cache.card = nullptr;
        return false;
    }
    SdBaseFile file;
    if (!file.open(kRuntimeTablePath, FILE_READ))
    {
        cache.card = nullptr;
        return false;
    }
    const Source source = {&file, readFile, file.fileSize()};
    const bool decoded = readRuntimeManifest(source, manifest);
    file.close();
    if (!decoded || !cache.matches(sd, manifest)) cache.card = nullptr;
    return decoded;
}

bool loadRuntimeIdleButtons(SdFat *sd, uint32_t offset, ButtonBindings &bindings)
{
    using namespace RuntimeTableInternal;
    SdBaseFile file;
    uint8_t row[20];
    if (sd == nullptr || !file.open(kRuntimeTablePath, FILE_READ))
        return false;
    const bool loaded = readFile(&file, offset, row, sizeof(row));
    file.close();
    if (!loaded)
        return false;
    bindings = {};
    bindings.buttonCount = kPetBehaviorButtonCount;
    for (uint8_t slot = 0; slot < kPetBehaviorButtonCount; ++slot)
    {
        auto &button = bindings.buttons[slot];
        button.active = true;
        button.kind = static_cast<PetBehaviorButtonKind>(row[4 + slot * 2]);
        const uint8_t target = row[5 + slot * 2];
        button.actionSlot = target;
        button.systemCommandId = static_cast<RuntimeSystemCommandId>(target);
#if ENABLE_PROJECT_MENUS
        button.menuSlot = target;
#endif
    }
    return true;
}
