#include "resources/RuntimeTableBehavior.h"

#include <limits.h>
#include <string.h>
#include "resources/detail/RuntimeTableFile.h"
#include "resources/detail/RuntimeTableDecoders.h"
#include "appearance/detail/RuntimeTableAppearanceDecoder.h"

using namespace RuntimeTableInternal;

namespace
{
struct MemorySource
{
    const uint8_t *bytes;
    uint32_t size;
};

bool readMemory(void *context, uint32_t offset, uint8_t *destination, size_t size)
{
    MemorySource &source = *static_cast<MemorySource *>(context);
    if (destination == nullptr || offset > source.size || size > source.size - offset)
        return false;
    memcpy(destination, source.bytes + offset, size);
    return true;
}

} // namespace

bool parseRuntimeTableBehavior(const uint8_t *bytes,
                               size_t byteCount,
                               const AssetData::RuntimeManifest &manifest,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config)
{
    if (bytes == nullptr || byteCount > UINT32_MAX)
        return false;
    MemorySource memory = {bytes, static_cast<uint32_t>(byteCount)};
    const Source source = {&memory, readMemory, memory.size};
    RuntimeTable table = {};
    PetBehaviorConfig candidate = {};
    if (!readRuntimeTable(source, &manifest, table) ||
        !decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, candidate) ||
        !decodeRuntimePresentation(table, speciesSlot, outfitSlot, candidate))
        return false;
    config = candidate;
    return true;
}

bool loadCompleteRuntimeTable(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              BundleReader &bundleReader,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config,
                              AppearanceSelection *initialAppearance,
                              bool *initialAppearanceResolved)
{
    // Keep one configuration on memory-constrained targets. Runtime-contract
    // failures are fatal to the session, so decode into the caller-owned buffer
    // instead of retaining a second configuration on the startup stack.
    config = {};
    if (initialAppearance != nullptr)
        *initialAppearance = {};
    if (initialAppearanceResolved != nullptr)
        *initialAppearanceResolved = false;
    RuntimeTableFile tableFile;
    if (!tableFile.open(sd, manifest))
        return false;
    const RuntimeTable &table = tableFile.table();

    AppearanceSelection decodedInitial = {};
    bool decoded = true;
    if (initialAppearance != nullptr)
    {
        decoded = decodeInitialAppearance(table, bundleReader, decodedInitial);
        if (decoded)
        {
            speciesSlot = decodedInitial.speciesSlot;
            outfitSlot = decodedInitial.outfitSlot;
            if (initialAppearanceResolved != nullptr)
                *initialAppearanceResolved = true;
        }
    }
    decoded = decoded &&
              decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, config) &&
              decodeRuntimePresentation(table, speciesSlot, outfitSlot, config);
    if (decoded && initialAppearance == nullptr)
        decoded = decodeInitialAppearance(table, bundleReader, decodedInitial);
    if (decoded)
        decoded = AssetData::animationReferenceExists(bundleReader, config.idleAnimation);
    // The former second /runtime.bin read only checked these required
    // appearance sections. Keep that check after used asset references.
    if (decoded)
        decoded = validateAppearanceSections(table);
    if (!decoded)
        return false;
    if (initialAppearance != nullptr)
        *initialAppearance = decodedInitial;
    return true;
}
