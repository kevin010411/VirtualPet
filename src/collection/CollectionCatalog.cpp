#include "collection/CollectionCatalog.h"
#include "resources/detail/RuntimeTableFile.h"
#include "pet/Pet.h"
#include "display/Renderer.h"

using namespace RuntimeTableInternal;

namespace
{
// These shared SD/render seams are used by progress and navigation. Keep one
// copy rather than expanding them at every call site.
__attribute__((noinline)) bool product(const RuntimeTable &table, const uint8_t *pair,
             AssetData::AnimationRef &ref, uint16_t &frame)
{
    const Section *assets = table.find(AssetRefs);
    frame = readU16(pair + 2);
    return assets != nullptr &&
        resolveAssetReference(table.source, *assets, readU16(pair), ref);
}

__attribute__((noinline)) bool show(Renderer &renderer, const RuntimeTable &table, const uint8_t *pair,
          uint8_t x, uint8_t y, uint16_t width, uint16_t height)
{
    AssetData::AnimationRef ref = {};
    uint16_t frame = 0;
    return product(table, pair, ref, frame) &&
        renderer.ShowDataFrame({ref.speciesSlot, ref.outfitSlot, ref.animationId, 0, frame},
                               x, y, 12, width, height);
}

__attribute__((noinline)) void activationTarget(const uint8_t *item,
                      uint8_t &speciesSlot, uint8_t &outfitSlot)
{
    // Export resolves both appearance modes to their final Species/Outfit pair.
    const uint16_t target = readU16(item + 30);
    speciesSlot = target >> 8;
    outfitSlot = target & 255;
}

__attribute__((noinline)) bool readItem(const RuntimeTable &table, uint16_t index, uint8_t *item)
{
    // Record buffers are consumed only after a complete successful read; avoid
    // emitting an unnecessary zero-fill before each streamed SD record.
    const Section *items = table.find(CollectionItems);
    return items != nullptr && readRecord(table.source, *items, index, item);
}

__attribute__((noinline)) bool applyGrants(const RuntimeTable &table, Pet &pet)
{
    const Section *items = table.find(CollectionItems);
    if (items == nullptr) return false;
    for (uint16_t index = 0; index < items->count; ++index)
    {
        if (!pet.isCollectionOwned(index)) continue;
        uint8_t item[36], species = 0, outfit = 0;
        if (!readItem(table, index, item)) return false;
        if (item[1] == 0) continue;
        activationTarget(item, species, outfit);
        if (species == pet.speciesSlot()) pet.unlockOutfit(outfit);
    }
    return true;
}

__attribute__((noinline)) bool header(const RuntimeTable &table, uint8_t *record)
{
    const Section *section = table.find(CollectionConfig);
    return section != nullptr &&
        readRecord(table.source, *section, 0, record);
}
}

bool loadCollectionCatalog(const RuntimeTable &table, PetBehaviorConfig &config)
{
    config.collectionFingerprint = 0;
    config.trackedActionFingerprint = 0;
    config.trackedActionCount = 0;
    if ((table.featureFlags & kCollectionFeature) == 0) return true;

    // The backend validates relationships and assets before export. Read only
    // the configuration and mappings needed by the firmware's fixed storage.
    uint8_t cfg[24];
    const Section *items = table.find(CollectionItems);
    const Section *tracked = table.find(CollectionTrackedActions);
    if (!header(table, cfg) || readU16(cfg + 16) == 0 || readU16(cfg + 16) > 64 ||
        items == nullptr || items->count > 64 || readU16(cfg + 18) > kTrackedActionCount ||
        tracked == nullptr || tracked->count > kTrackedActionCount) return false;
    for (uint8_t index = 0; index < tracked->count; ++index)
    {
        uint8_t row[4];
        if (!readRecord(table.source, *tracked, index, row)) return false;
        config.trackedActions[index] = row[1];
    }
    config.collectionFingerprint = readU32(cfg);
    config.trackedActionFingerprint = readU32(cfg + 20);
    config.trackedActionCount = tracked->count;
    return true;
}

namespace
{
// Grant-only queries share the table lifetime without acquiring items or
// changing fingerprints. Acquisition completes before grants are applied.
__attribute__((noinline)) bool updateCollectionCatalog(SdFat *sd, const PetBehaviorConfig &config, Pet &pet, bool *changed)
{
    if (changed != nullptr)
    {
        *changed = pet.configureCollection(config.collectionFingerprint);
        *changed |= pet.configureTrackedActions(config.trackedActionFingerprint);
    }
    if (config.collectionFingerprint == 0) return true;
    RuntimeTableFile file;
    if (!file.open(sd, config.assetManifest)) return false;
    const RuntimeTable &table = file.table();
    if (changed == nullptr) return applyGrants(table, pet);
    const Section *items = table.find(CollectionItems);
    const Section *conditions = table.find(CollectionConditions);
    if (items == nullptr || conditions == nullptr) return false;
    // Acquisition changes only ownership. Source members remain stable
    // until all clauses finish; appearance grants run after this loop.
    const PersistedPetState &state = pet.persistentState();
    int32_t identities[2];
    identities[0] = pet.speciesSlot();
    identities[1] = identities[0] * 256 + pet.outfitSlot();
    for (uint16_t index = 0; index < items->count; ++index)
    {
        if (pet.isCollectionOwned(index)) continue;
        uint8_t item[36];
        if (!readItem(table, index, item)) return false;
        bool matches = true;
        for (uint16_t clause = 0; clause < readU16(item + 4); ++clause)
        {
            uint8_t condition[18];
            if (!readRecord(table.source, *conditions, readU16(item + 2) + clause, condition)) return false;
            if (condition[0] >= kPetCustomStatCount + 2 + kTrackedActionCount) return false;
            const uint8_t source = condition[0];
            const int64_t value = source < kPetCustomStatCount ? state.customStats[source] :
                source >= kPetCustomStatCount + 2 ? state.actionCounts[source - kPetCustomStatCount - 2] : identities[source - kPetCustomStatCount];
            matches &= value >= readI64(condition + 2) && value <= readI64(condition + 10);
        }
        if (matches) { pet.acquireCollection(index); *changed = true; }
    }
    const uint8_t previousMask = pet.outfitUnlockMask();
    if (!applyGrants(table, pet)) return false;
    *changed |= pet.outfitUnlockMask() != previousMask;
    return true;
}
}

bool evaluateCollectionCatalog(SdFat *sd, const PetBehaviorConfig &config, Pet &pet, bool &changed)
{
    return updateCollectionCatalog(sd, config, pet, &changed);
}

bool applyCollectionOutfitGrants(SdFat *sd, const PetBehaviorConfig &config, Pet &pet)
{
    return updateCollectionCatalog(sd, config, pet, nullptr);
}

CollectionCatalog::CollectionCatalog(Pet &petRef, Renderer &rendererRef, const PetBehaviorConfig &configRef)
    : pet(petRef), renderer(rendererRef), config(configRef) {}

bool CollectionCatalog::start()
{
    if (config.collectionFingerprint == 0) return false;
    RuntimeTableFile file;
    uint8_t cfg[24];
    if (!file.open(renderer.sdCard(), config.assetManifest) || !header(file.table(), cfg)) return false;
    count = static_cast<uint8_t>(readU16(cfg + 16));
    opened = true;
    targetSpecies = targetOutfit = 0;
    detail = false;
    detailPage = 0;
    focus = 0;
    page = 0;
    return redraw();
}

__attribute__((noinline)) bool CollectionCatalog::drawCell(const RuntimeTable &table, uint8_t cell, bool selected)
{
    const uint8_t x = (cell % 4) * 32;
    const uint8_t y = (cell / 4) * 32;
    if (cell == 0)
    {
        uint8_t cfg[24];
        return header(table, cfg) && show(renderer, table, cfg + (selected ? 12 : 8), x, y, 32, 32);
    }
    const uint8_t slot = page * 15 + cell - 1;
    if (slot >= count) return true;
    uint8_t item[36];
    return readItem(table, slot, item) &&
        show(renderer, table, item + 6 + (pet.isCollectionOwned(slot) ? 8 : 0) +
             (selected ? 4 : 0), x, y, 32, 32);
}

bool CollectionCatalog::redraw()
{
    if (!opened) return true;
    RuntimeTableFile file;
    uint8_t cfg[24];
    if (!file.open(renderer.sdCard(), config.assetManifest) || !header(file.table(), cfg)) return false;
    if (!show(renderer, file.table(), cfg + 4, 0, 0, 128, 160)) return false;
    for (uint8_t cell = 0; cell < 16; ++cell)
        if (!drawCell(file.table(), cell, cell == focus)) return false;
    if (detail)
    {
        uint8_t item[36];
        const uint8_t slot = page * 15 + focus - 1;
        if (!readItem(file.table(), slot, item)) return false;
        uint8_t *pair = item + (pet.isCollectionOwned(slot) ? 26 : 22);
        const uint16_t frame = readU16(pair + 2) + detailPage;
        pair[2] = frame & 255; pair[3] = frame >> 8;
        return show(renderer, file.table(), pair, 0, 112, 128, 48);
    }
    return true;
}

bool CollectionCatalog::move(bool right)
{
    if (detail) return true;
    const uint8_t previous = focus;
    const uint8_t last = (count - page * 15) < 15 ? count - page * 15 : 15;
    if (right && focus == last)
    {
        page = (page + 1) % ((count + 14) / 15); focus = 0; return redraw();
    }
    if (!right && focus == 0)
    {
        page = page == 0 ? (count - 1) / 15 : page - 1;
        focus = (count - page * 15) < 15 ? count - page * 15 : 15;
        return redraw();
    }
    focus = right ? focus + 1 : focus - 1;
    RuntimeTableFile file;
    return file.open(renderer.sdCard(), config.assetManifest) &&
        drawCell(file.table(), previous, false) && drawCell(file.table(), focus, true);
}

bool CollectionCatalog::confirm()
{
    if (detail)
    {
        RuntimeTableFile file;
        uint8_t item[36];
        if (!file.open(renderer.sdCard(), config.assetManifest) ||
            !readItem(file.table(), page * 15 + focus - 1, item)) return false;
        const uint16_t pages = readU16(item + (pet.isCollectionOwned(page * 15 + focus - 1) ? 34 : 32));
        if (++detailPage < pages) return redraw();
        detail = false; detailPage = 0;
        return redraw();
    }
    if (focus == 0) { exit(); return true; }
    const uint8_t slot = page * 15 + focus - 1;
    RuntimeTableFile file;
    uint8_t item[36];
    if (!file.open(renderer.sdCard(), config.assetManifest) ||
        !readItem(file.table(), slot, item)) return false;
    if (pet.isCollectionOwned(slot))
    {
        if (item[1] != 0)
        {
            activationTarget(item, targetSpecies, targetOutfit);
            exit();
            return true;
        }
    }
    if (readU16(item + (pet.isCollectionOwned(slot) ? 34 : 32)) == 0) return true;
    detailPage = 0;
    detail = true;
    return redraw();
}

bool CollectionCatalog::takeActivation(uint8_t &species, uint8_t &outfit)
{
    if (targetSpecies == 0) return false;
    species = targetSpecies;
    outfit = targetOutfit;
    targetSpecies = targetOutfit = 0;
    return true;
}
