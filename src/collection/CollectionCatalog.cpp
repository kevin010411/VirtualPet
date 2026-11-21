#include "resources/AssetResources.h"
#include "collection/CollectionCatalog.h"
#include "collection/CollectionGameEvent.h"
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
          uint8_t x, uint8_t y, uint16_t width, uint16_t height, const DisplayRect *crop = nullptr)
{
    AssetData::AnimationRef ref = {};
    uint16_t frame = 0;
    return product(table, pair, ref, frame) &&
        renderer.ShowDataFrame({ref.speciesSlot, ref.outfitSlot, ref.animationId, 0, frame},
                               x, y, 12, width, height, crop);
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
#if ENABLE_COLLECTION_GAME_EVENTS
    config.retainCollectionOnRestart = false;
    config.captureOnlyMask = 0;
    config.collectionProgressFingerprint = 0;
#endif
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
#if ENABLE_COLLECTION_GAME_EVENTS
    if (table.featureFlags & kCollectionGameEventsFeature)
    {
        const Section *gameConfig = table.find(CollectionGameConfig);
        uint8_t row[16];
        if (gameConfig == nullptr || !readRecord(table.source, *gameConfig, 0, row)) return false;
        config.captureOnlyMask = static_cast<uint64_t>(readI64(row));
        config.collectionProgressFingerprint = readU32(row + 8);
        config.retainCollectionOnRestart = row[12] != 0;
    }
#endif
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
#if ENABLE_COLLECTION_GAME_EVENTS
        *changed |= pet.configureCollectionProgress(config.collectionProgressFingerprint);
#endif
    }
    if (config.collectionFingerprint == 0) return true;
    RuntimeTableFile file;
    if (!file.open(sd, config.assetManifest)) return false;
    const RuntimeTable &table = file.table();
    if (changed == nullptr) return applyGrants(table, pet);
    const Section *items = table.find(CollectionItems);
    const Section *conditions = table.find(CollectionConditions);
    if (items == nullptr || conditions == nullptr) return false;
    // Appearance grants run after acquisition so all non-ownership values
    // stay stable while dependent collections are evaluated.
    const PersistedPetState &state = pet.persistentState();
    const int32_t identities[] = {pet.speciesSlot(), pet.speciesSlot() * 256 + pet.outfitSlot()};
#if ENABLE_COLLECTION_GAME_EVENTS
    bool acquired;
    do
    {
        acquired = false;
#endif
        for (uint16_t index = 0; index < items->count; ++index)
        {
            if (pet.isCollectionOwned(index)) continue;
#if ENABLE_COLLECTION_GAME_EVENTS
            if (config.captureOnlyMask & (uint64_t(1) << index)) continue;
#endif
            uint8_t item[36];
            if (!readItem(table, index, item)) return false;
            bool matches = true;
            for (uint16_t clause = 0; clause < readU16(item + 4); ++clause)
            {
                uint8_t condition[18];
                if (!readRecord(table.source, *conditions, readU16(item + 2) + clause, condition)) return false;
                const uint8_t source = condition[0];
#if ENABLE_COLLECTION_GAME_EVENTS
                if (source == 22)
                {
                    const uint64_t required = static_cast<uint64_t>(readI64(condition + 2));
                    matches &= (state.collectionMask & required) == required;
                    continue;
                }
                if (source == 23)
                {
                    matches &= pet.collectionProgress(condition[1]) >= readU32(condition + 2);
                    continue;
                }
#endif
                if (source >= kPetCustomStatCount + 2 + kTrackedActionCount) return false;
                const int64_t value = source < kPetCustomStatCount ? state.customStats[source] :
                    source >= kPetCustomStatCount + 2 ? state.actionCounts[source - kPetCustomStatCount - 2] : identities[source - kPetCustomStatCount];
                matches &= value >= readI64(condition + 2) && value <= readI64(condition + 10);
            }
            if (matches)
            {
                pet.acquireCollection(index);
                *changed = true;
#if ENABLE_COLLECTION_GAME_EVENTS
                acquired = true;
#endif
            }
        }
#if ENABLE_COLLECTION_GAME_EVENTS
    } while (acquired && (table.featureFlags & kCollectionGameEventsFeature) != 0);
#endif
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

#if ENABLE_COLLECTION_GAME_EVENTS
namespace
{
constexpr uint8_t noShopItem = 255;
constexpr uint8_t stageDaysSlot = 255;
enum class RuleKind : uint8_t { UpTo = 1, DownTo, Decrease, Round, Earned };
enum class RoundMetric : uint8_t { Catches, GroupCatches, OutsideGroup, DistinctInGroup, EndingStat, StatIncreases };

uint32_t statValue(const PersistedPetState &state, uint8_t slot)
{
    return slot == stageDaysSlot ? state.stage_days : state.customStats[slot];
}

uint32_t metricValue(const CollectionRoundStats &round, const PersistedPetState &state,
                     RoundMetric metric, uint8_t slot)
{
    switch (metric)
    {
    case RoundMetric::Catches: return round.catches;
    case RoundMetric::GroupCatches: return round.groupCatches[slot];
    case RoundMetric::OutsideGroup: return round.catches - round.groupCatches[slot];
    case RoundMetric::DistinctInGroup: return round.distinctInGroup[slot];
    case RoundMetric::EndingStat: return statValue(state, slot);
    case RoundMetric::StatIncreases:
        return round.statIncreases[slot == stageDaysSlot ? kPetCustomStatCount : slot];
    }
    return 0;
}

bool roundMatches(const RuntimeTable &table, const uint8_t *rule, const PersistedPetState &state,
                  const CollectionRoundStats &round, bool &matches)
{
    const Section *requirements = table.find(CollectionRoundRequirements);
    if (requirements == nullptr) return false;
    matches = true;
    for (uint8_t index = 0; index < rule[3]; ++index)
    {
        uint8_t req[8];
        if (!readRecord(table.source, *requirements, readU16(rule + 4) + index, req)) return false;
        const uint32_t amount = metricValue(round, state, static_cast<RoundMetric>(req[0]), req[2]);
        const uint32_t threshold = readU32(req + 4);
        if (req[1] == 0 ? amount != threshold : amount < threshold)
        {
            matches = false;
            break;
        }
    }
    return true;
}

uint32_t statIncrement(const uint8_t *rule, const PersistedPetState &before,
                       const PersistedPetState &after, CollectionGameEvent event, uint8_t shopItem)
{
    const RuleKind kind = static_cast<RuleKind>(rule[0]);
    const uint8_t filter = rule[2];
    if (filter != noShopItem && shopItem != filter) return 0;
    if (kind == RuleKind::Earned && event == CollectionGameEvent::StatTransition) return 0;
    const uint32_t oldValue = statValue(before, rule[1]);
    const uint32_t newValue = statValue(after, rule[1]);
    const uint32_t threshold = readU32(rule + 12);
    switch (kind)
    {
    case RuleKind::UpTo: return oldValue < threshold && newValue >= threshold;
    case RuleKind::DownTo: return oldValue > threshold && newValue <= threshold;
    case RuleKind::Decrease: return oldValue >= newValue && oldValue - newValue >= threshold;
    case RuleKind::Earned: return newValue > oldValue ? newValue - oldValue : 0;
    case RuleKind::Round: break;
    }
    return 0;
}
}

bool recordCollectionGameEvent(SdFat *sd, const PetBehaviorConfig &config, Pet &pet,
                               const PersistedPetState &before, CollectionGameEvent event,
                               uint8_t shopItem, const CollectionRoundStats *round)
{
    if (config.collectionProgressFingerprint == 0 || pet.isDead()) return true;
    RuntimeTableFile file;
    if (!file.open(sd, config.assetManifest)) return false;
    const RuntimeTable &table = file.table();
    const Section *rules = table.find(CollectionEventRules);
    if (rules == nullptr) return false;

    pet.configureCollectionProgress(config.collectionProgressFingerprint);
    const PersistedPetState &after = pet.persistentState();
    for (uint16_t slot = 0; slot < rules->count; ++slot)
    {
        uint8_t rule[16];
        if (!readRecord(table.source, *rules, slot, rule)) return false;
        const uint32_t goal = readU32(rule + 8);
        const uint32_t progress = pet.collectionProgress(slot);
        if (progress >= goal) continue;

        uint32_t increment;
        if (static_cast<RuleKind>(rule[0]) == RuleKind::Round)
        {
            if (event != CollectionGameEvent::CaptureRound || round == nullptr) continue;
            bool matches;
            if (!roundMatches(table, rule, after, *round, matches)) return false;
            increment = matches;
        }
        else
            increment = statIncrement(rule, before, after, event, shopItem);

        if (increment)
            pet.setCollectionProgress(slot, increment >= goal - progress ? goal : progress + increment);
    }
    return true;
}
#endif

CollectionCatalog::CollectionCatalog(Pet &petRef, Renderer &rendererRef, AssetResources &resourcesRef, const PetBehaviorConfig &configRef)
    : pet(petRef), renderer(rendererRef), resources(resourcesRef), config(configRef) {}

bool CollectionCatalog::start()
{
    if (config.collectionFingerprint == 0) return false;
    RuntimeTableFile file;
    uint8_t cfg[24];
    if (!file.open(resources.sdCard(), config.assetManifest) || !header(file.table(), cfg)) return false;
    AssetData::AnimationRef ref = {};
    AssetData::AnimationRecord chrome = {};
    uint16_t frame = 0;
    if (!product(file.table(), cfg + 4, ref, frame) || !resources.resolveAnimation(ref, 0, chrome))
        return false;
    backgroundBands = chrome.frameCount > 3;
    count = static_cast<uint8_t>(readU16(cfg + 16));
    opened = true;
    targetSpecies = targetOutfit = 0;
    detail = false;
    detailPage = 0;
    focus = 0;
    page = 0;
    return redraw();
}

uint8_t CollectionCatalog::lastCell() const
{
    const uint8_t remaining = count - page * 15;
    return remaining < 15 ? remaining : 15;
}

__attribute__((noinline)) bool CollectionCatalog::drawCell(const RuntimeTable &table, uint8_t cell,
                                                           bool selected, const DisplayRect *crop)
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
             (selected ? 4 : 0), x, y, 32, 32, crop);
}

bool CollectionCatalog::drawDetail(const RuntimeTable &table, uint8_t *item)
{
    const uint8_t slot = page * 15 + focus - 1;
    uint8_t *pair = item + (pet.isCollectionOwned(slot) ? 26 : 22);
    const uint16_t frame = readU16(pair + 2) + detailPage;
    pair[2] = frame & 255;
    pair[3] = frame >> 8;
    return show(renderer, table, pair, 0, 112, 128, 48);
}

bool CollectionCatalog::restoreBackground(const RuntimeTable &table, const uint8_t *cfg, DisplayRect rect)
{
    if (!backgroundBands) return show(renderer, table, cfg + 4, 0, 0, 128, 160, &rect);

    const bool detailBand = rect.y >= 112;
    const uint8_t top = detailBand ? 112 : (rect.y / 32) * 32;
    const uint8_t height = detailBand ? 48 : 32;
    const uint8_t pair[] = {cfg[4], cfg[5], static_cast<uint8_t>(detailBand ? 7 : 3 + top / 32), 0};
    rect.y -= top;
    const bool wholeBand = rect.w == 128 && rect.h == height;
    return show(renderer, table, pair, 0, top, 128, height, wholeBand ? nullptr : &rect);
}

bool CollectionCatalog::restoreDetail(const RuntimeTable &table)
{
    uint8_t cfg[24];
    if (!header(table, cfg)) return false;
    const uint8_t last = lastCell();
    if (last < 12) return restoreBackground(table, cfg, {0, 112, 128, 48});

    const DisplayRect lowerHalf{0, 16, 32, 16};
    for (uint8_t cell = 12; cell <= last; ++cell)
        if (!drawCell(table, cell, cell == focus, &lowerHalf)) return false;
    const uint8_t usedWidth = (last - 11) * 32;
    if (usedWidth < 128 && !restoreBackground(table, cfg,
        {usedWidth, 112, static_cast<uint8_t>(128 - usedWidth), 16})) return false;
    return restoreBackground(table, cfg, {0, 128, 128, 32});
}

bool CollectionCatalog::redraw()
{
    if (!opened) return true;
    RuntimeTableFile file;
    uint8_t cfg[24];
    if (!file.open(resources.sdCard(), config.assetManifest) || !header(file.table(), cfg)) return false;
    if (!show(renderer, file.table(), cfg + 4, 0, 0, 128, 160)) return false;
    for (uint8_t cell = 0; cell <= lastCell(); ++cell)
        if (!drawCell(file.table(), cell, cell == focus)) return false;
    if (detail)
    {
        uint8_t item[36];
        const uint8_t slot = page * 15 + focus - 1;
        return readItem(file.table(), slot, item) && drawDetail(file.table(), item);
    }
    return true;
}

bool CollectionCatalog::move(bool right)
{
    if (detail) return true;
    const uint8_t previous = focus;
    const uint8_t previousPage = page;
    const uint8_t last = lastCell();
    if (right && focus == last)
    {
        page = (page + 1) % ((count + 14) / 15);
        focus = 0;
    }
    else if (!right && focus == 0)
    {
        page = page == 0 ? (count - 1) / 15 : page - 1;
        focus = lastCell();
    }
    else focus = right ? focus + 1 : focus - 1;
    RuntimeTableFile file;
    if (!file.open(resources.sdCard(), config.assetManifest)) return false;
    const auto &table = file.table();
    if (page == previousPage)
        return drawCell(table, previous, false) && drawCell(table, focus, true);

    if ((previous == 0) != (focus == 0) && !drawCell(table, 0, focus == 0)) return false;
    const uint8_t currentLast = lastCell();
    for (uint8_t cell = 1; cell <= currentLast; ++cell)
        if (!drawCell(table, cell, cell == focus)) return false;
    if (currentLast >= last) return true;

    uint8_t cfg[24];
    if (!header(table, cfg)) return false;
    // Clear only cells vacated on the shorter page, one strip per grid row.
    for (uint8_t cell = currentLast + 1; cell <= last;)
    {
        const uint8_t x = (cell % 4) * 32;
        uint8_t cells = 4 - cell % 4;
        if (cells > last - cell + 1) cells = last - cell + 1;
        if (!restoreBackground(table, cfg, {x, static_cast<uint8_t>(cell / 4 * 32),
                                            static_cast<uint8_t>(cells * 32), 32})) return false;
        cell += cells;
    }
    return true;
}

bool CollectionCatalog::confirm()
{
    if (detail)
    {
        RuntimeTableFile file;
        uint8_t item[36];
        if (!file.open(resources.sdCard(), config.assetManifest) ||
            !readItem(file.table(), page * 15 + focus - 1, item)) return false;
        const uint16_t pages = readU16(item + (pet.isCollectionOwned(page * 15 + focus - 1) ? 34 : 32));
        if (++detailPage < pages) return drawDetail(file.table(), item);
        detail = false; detailPage = 0;
        return restoreDetail(file.table());
    }
    if (focus == 0) { exit(); return true; }
    const uint8_t slot = page * 15 + focus - 1;
    RuntimeTableFile file;
    uint8_t item[36];
    if (!file.open(resources.sdCard(), config.assetManifest) ||
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
    return drawDetail(file.table(), item);
}

bool CollectionCatalog::takeActivation(uint8_t &species, uint8_t &outfit)
{
    if (targetSpecies == 0) return false;
    species = targetSpecies;
    outfit = targetOutfit;
    targetSpecies = targetOutfit = 0;
    return true;
}
