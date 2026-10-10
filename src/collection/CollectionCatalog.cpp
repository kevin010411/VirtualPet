#include "collection/CollectionCatalog.h"
#include "resources/detail/RuntimeTableFile.h"
#include "pet/Pet.h"
#include "display/Renderer.h"
#include "display/FrameDecoder.h"

using namespace RuntimeTableInternal;

namespace
{
bool product(const RuntimeTable &table, const uint8_t *pair,
             AssetData::AnimationRef &ref, uint16_t &frame)
{
    const Section *assets = table.find(AssetRefs);
    frame = readU16(pair + 2);
    return assets != nullptr && frame < 256 &&
        resolveAssetReference(table.source, *assets, readU16(pair), ref);
}

bool show(Renderer &renderer, const RuntimeTable &table, const uint8_t *pair,
          uint8_t x, uint8_t y, uint16_t width, uint16_t height)
{
    AssetData::AnimationRef ref = {};
    uint16_t frame = 0;
    return product(table, pair, ref, frame) &&
        renderer.ShowDataFrame({ref.speciesSlot, ref.outfitSlot, ref.animationId, 0, frame},
                               x, y, 12, width, height);
}

bool header(const RuntimeTable &table, uint8_t *record)
{
    const Section *section = table.find(CollectionConfig);
    return section != nullptr && section->count == 1 &&
        readRecord(table.source, *section, 0, record);
}
}

bool validateCollectionCatalog(const RuntimeTable &table, uint8_t statCount,
                               PetBehaviorConfig &config)
{
    config.collectionFingerprint = 0;
    config.trackedActionFingerprint = 0;
    config.trackedActionCount = 0;
    const bool enabled = (table.featureFlags & kCollectionFeature) != 0;
    const Section *items = table.find(CollectionItems);
    const Section *conditions = table.find(CollectionConditions);
    if (!enabled)
        return table.find(CollectionConfig) == nullptr && items == nullptr && conditions == nullptr && table.find(CollectionTrackedActions) == nullptr;
#if !ENABLE_COLLECTION_CATALOG
    return false;
#else
    uint8_t cfg[24] = {};
    if (!header(table, cfg) || items == nullptr || conditions == nullptr ||
        items->count == 0 || items->count > 64 || conditions->count > 384 ||
        readU16(cfg + 16) != items->count || readU16(cfg + 18) > 8 || readU32(cfg) == 0)
        return false;
    for (uint8_t index = 0; index < 3; ++index)
    {
        AssetData::AnimationRef ref = {};
        uint16_t frame = 0;
        if (!product(table, cfg + 4 + index * 4, ref, frame)) return false;
    }
    const Section *tracked = table.find(CollectionTrackedActions);
    const Section *actions = table.find(Actions);
    if (tracked == nullptr || actions == nullptr || tracked->count != readU16(cfg + 18) ||
        (tracked->count == 0) != (readU32(cfg + 20) == 0)) return false;
    uint8_t actionMask = 0;
    for (uint8_t index = 0; index < tracked->count; ++index)
    {
        uint8_t row[4] = {};
        if (!readRecord(table.source, *tracked, index, row) || row[0] != index ||
            row[1] >= 8 || readU16(row + 2) != 0 || (actionMask & (1U << row[1]))) return false;
        bool found = false;
        for (uint16_t action = 0; action < actions->count; ++action)
        {
            uint8_t record[16] = {};
            if (!readRecord(table.source, *actions, action, record)) return false;
            found |= record[0] == row[1];
        }
        if (!found) return false;
        config.trackedActions[index] = row[1];
        actionMask |= 1U << row[1];
    }
    uint8_t usedTracked = 0;
    uint16_t consumed = 0;
    for (uint16_t index = 0; index < items->count; ++index)
    {
        uint8_t item[30] = {};
        if (!readRecord(table.source, *items, index, item) || item[0] != index || item[1] != 0 ||
            readU16(item + 2) != consumed || readU16(item + 4) == 0 || readU16(item + 4) > 6 ||
            consumed + readU16(item + 4) > conditions->count)
            return false;
        for (uint8_t image = 0; image < 6; ++image)
        {
            AssetData::AnimationRef ref = {};
            uint16_t frame = 0;
            if (!product(table, item + 6 + image * 4, ref, frame)) return false;
        }
        for (uint8_t clause = 0; clause < readU16(item + 4); ++clause)
        {
            uint8_t condition[10] = {};
            if (!readRecord(table.source, *conditions, consumed++, condition) ||
                readI32(condition + 2) > readI32(condition + 6)) return false;
            const int32_t minimum = readI32(condition + 2), maximum = readI32(condition + 6);
            if (condition[1] == 0)
            {
                if (condition[0] >= statCount || condition[0] >= kPetCustomStatCount) return false;
                uint8_t stat[12] = {};
                const Section *stats = table.find(PetStats);
                if (stats == nullptr || !readRecord(table.source, *stats, condition[0], stat) ||
                    minimum < readI16(stat + 4) || maximum > readI16(stat + 6)) return false;
            }
            else if (condition[1] == 1 || condition[1] == 2)
            {
                if (condition[0] != 0 || minimum != maximum || minimum <= 0 || minimum > 65535) return false;
                const Section *identities = table.find(condition[1] == 1 ? Species : Outfits);
                bool found = false;
                if (identities == nullptr) return false;
                for (uint16_t identity = 0; identity < identities->count; ++identity)
                {
                    uint8_t record[8] = {};
                    if (!readRecord(table.source, *identities, identity, record)) return false;
                    const int32_t target = condition[1] == 1 ? record[0] : record[0] * 256 + record[1];
                    found |= target == minimum;
                }
                if (!found) return false;
            }
            else if (condition[1] == 3)
            {
                if (condition[0] >= tracked->count || minimum < 1 || maximum != 65535) return false;
                usedTracked |= 1U << condition[0];
            }
            else return false;
        }
    }
    config.collectionFingerprint = readU32(cfg);
    config.trackedActionFingerprint = readU32(cfg + 20);
    config.trackedActionCount = tracked->count;
    return consumed == conditions->count && usedTracked == ((1U << tracked->count) - 1U);
#endif
}

bool evaluateCollectionCatalog(SdFat *sd, const PetBehaviorConfig &config, Pet &pet, bool &changed)
{
    changed = pet.configureCollection(config.collectionFingerprint);
    changed |= pet.configureTrackedActions(config.trackedActionFingerprint);
    if (config.collectionFingerprint == 0) return true;
    RuntimeTableFile file;
    if (!file.open(sd, config.assetManifest)) return false;
    const RuntimeTable &table = file.table();
    const Section *items = table.find(CollectionItems);
    const Section *conditions = table.find(CollectionConditions);
    if (items == nullptr || conditions == nullptr) return false;
    const PetStatSnapshot snapshot = pet.statSnapshot();
    uint16_t counts[8] = {};
    for (uint8_t index = 0; index < 8; ++index) counts[index] = pet.actionCount(index);
    for (uint16_t index = 0; index < items->count; ++index)
    {
        if (pet.isCollectionOwned(index)) continue;
        uint8_t item[30] = {};
        if (!readRecord(table.source, *items, index, item)) return false;
        bool matches = true;
        for (uint16_t clause = 0; clause < readU16(item + 4); ++clause)
        {
            uint8_t condition[10] = {};
            if (!readRecord(table.source, *conditions, readU16(item + 2) + clause, condition) ||
                condition[1] > 3) return false;
            int32_t value = 0;
            if (condition[1] == 0)
            {
                if (condition[0] >= config.statCount || condition[0] >= kPetCustomStatCount) return false;
                value = snapshot.customStats[condition[0]];
            }
            else if (condition[1] == 1) value = snapshot.speciesSlot;
            else if (condition[1] == 2) value = snapshot.speciesSlot * 256 + snapshot.outfitSlot;
            else
            {
                if (condition[0] >= config.trackedActionCount) return false;
                value = counts[condition[0]];
            }
            matches &= value >= readI32(condition + 2) && value <= readI32(condition + 6);
        }
        if (matches) { pet.acquireCollection(index); changed = true; }
    }
    return true;
}

bool validateCollectionProducts(const RuntimeTable &table, BundleReader &reader)
{
    if ((table.featureFlags & kCollectionFeature) == 0) return true;
    alignas(uint16_t) uint8_t buffer[FrameDecoder::kDataReadBufferBytes] = {};
    uint16_t line[128] = {};
    const auto valid = [&](const uint8_t *pair, uint16_t width, uint16_t height) {
        AssetData::AnimationRef ref = {};
        uint16_t index = 0;
        return product(table, pair, ref, index) && FrameDecoder::showDataFrame(reader,
            {ref.speciesSlot, ref.outfitSlot, ref.animationId, 0, index}, nullptr,
            buffer, sizeof(buffer), line, 128, 0, 0, 1, width, height);
    };
    uint8_t cfg[24] = {};
    if (!header(table, cfg) || !valid(cfg + 4, 128, 160) ||
        !valid(cfg + 8, 32, 32) || !valid(cfg + 12, 32, 32)) return false;
    const Section *items = table.find(CollectionItems);
    if (items == nullptr) return false;
    for (uint16_t index = 0; index < items->count; ++index)
    {
        uint8_t item[30] = {};
        if (!readRecord(table.source, *items, index, item)) return false;
        for (uint8_t image = 0; image < 6; ++image)
            if (!valid(item + 6 + image * 4, image < 4 ? 32 : 128, image < 4 ? 32 : 160)) return false;
    }
    return true;
}

CollectionCatalog::CollectionCatalog(Pet &petRef, Renderer &rendererRef, const PetBehaviorConfig &configRef)
    : pet(petRef), renderer(rendererRef), config(configRef) {}

bool CollectionCatalog::start()
{
    if (config.collectionFingerprint == 0) return false;
    RuntimeTableFile file;
    uint8_t cfg[24] = {};
    if (!file.open(renderer.sdCard(), config.assetManifest) || !header(file.table(), cfg)) return false;
    count = static_cast<uint8_t>(readU16(cfg + 16));
    opened = true;
    detail = false;
    focus = 0;
    page = 0;
    return redraw();
}

bool CollectionCatalog::drawCell(const RuntimeTable &table, uint8_t cell, bool selected)
{
    const uint8_t x = (cell % 4) * 32;
    const uint8_t y = (cell / 4) * 32;
    if (cell == 0)
    {
        uint8_t cfg[24] = {};
        return header(table, cfg) && show(renderer, table, cfg + (selected ? 12 : 8), x, y, 32, 32);
    }
    const uint8_t slot = page * 15 + cell - 1;
    if (slot >= count) return true;
    uint8_t item[30] = {};
    const Section *items = table.find(CollectionItems);
    return items != nullptr && readRecord(table.source, *items, slot, item) &&
        show(renderer, table, item + 6 + (pet.isCollectionOwned(slot) ? 8 : 0) +
             (selected ? 4 : 0), x, y, 32, 32);
}

bool CollectionCatalog::redraw()
{
    if (!opened) return true;
    RuntimeTableFile file;
    uint8_t cfg[24] = {};
    if (!file.open(renderer.sdCard(), config.assetManifest) || !header(file.table(), cfg)) return false;
    if (detail)
    {
        uint8_t item[30] = {};
        const uint8_t slot = page * 15 + focus - 1;
        const Section *items = file.table().find(CollectionItems);
        return items != nullptr && readRecord(file.table().source, *items, slot, item) &&
            show(renderer, file.table(), item + (pet.isCollectionOwned(slot) ? 26 : 22), 0, 0, 128, 160);
    }
    if (!show(renderer, file.table(), cfg + 4, 0, 0, 128, 160)) return false;
    for (uint8_t cell = 0; cell < 16; ++cell)
        if (!drawCell(file.table(), cell, cell == focus)) return false;
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
    if (detail) { detail = false; return redraw(); }
    if (focus == 0) { exit(); return true; }
    detail = true;
    return redraw();
}
