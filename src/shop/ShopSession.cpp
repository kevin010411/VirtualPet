#include "shop/ShopSession.h"
#if ENABLE_SHOP

#include <string.h>
#include "appearance/AppearanceChangeController.h"
#include "common/SdBinaryRead.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorTypes.h"
#include "pet/PetSaveController.h"
#include "resources/AssetResources.h"
#include "resources/detail/RuntimeTableReader.h"

using RuntimeTableInternal::readU16;
using RuntimeTableInternal::readU32;
using RuntimeTableInternal::readI64;

namespace
{
constexpr char path[] = "/shop.data";
constexpr uint8_t headerSize = 39;
constexpr uint8_t itemSize = 129;
constexpr uint8_t productSize = 70;
constexpr uint8_t navigation = 128;
constexpr uint8_t exitEntry = 129;
constexpr uint8_t firstEntryFrame = 16;

class ShopFile : public SdBaseFile
{
public:
    ~ShopFile() { close(); }
};
}

ShopSession::ShopSession(Pet &pet, PetSaveController &saves, AppearanceChangeController &appearance,
                         Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config)
    : pet(pet), saves(saves), appearance(appearance), renderer(renderer), resources(resources), config(config)
{
}

bool ShopSession::read(SdBaseFile &file, uint32_t offset, uint8_t *row, size_t length)
{
    if (file.seekSet(offset) && readSdBinary(file, row, length) == static_cast<int>(length))
        return true;
    resources.recordAssetDataErrorResource(path);
    return false;
}

bool ShopSession::configure()
{
    if (!config.shopEnabled)
    {
        art = 0;
        return true;
    }
    if (art && AssetData::sameBundleId(bundle, config.assetManifest.bundleId))
        return true;
    ShopFile file;
    uint8_t row[headerSize];
    if (!file.open(path, FILE_READ) || !read(file, 0, row, sizeof(row)))
    {
        resources.recordAssetDataErrorResource(path);
        return false;
    }
    memcpy(bundle.bytes, row, sizeof(bundle.bytes));
    if (!AssetData::sameBundleId(bundle, config.assetManifest.bundleId))
    {
        resources.recordAssetDataErrorResource(path);
        return false;
    }
    fingerprint = readU32(row + 16);
    art = readU16(row + 20);
    wallet = row[22];
    itemCount = row[23];
    productCount = row[24];
    rowHeight = row[25];
    speciesCount = row[26];
    memcpy(&walletRect, row + 27, sizeof(Rect));
    memcpy(&quantityRect, row + 31, sizeof(Rect));
    memcpy(&listRect, row + 35, sizeof(Rect));
    return true;
}

uint32_t ShopSession::itemOffset(uint8_t slot) const
{
    return headerSize + static_cast<uint32_t>(itemSize + 2 * speciesCount) * slot;
}

bool ShopSession::item(SdBaseFile &file, uint8_t slot, uint8_t *row)
{
    return read(file, itemOffset(slot), row, itemSize);
}

bool ShopSession::product(SdBaseFile &file, uint8_t slot, uint8_t *row)
{
    return read(file, itemOffset(itemCount) + static_cast<uint32_t>(productSize) * slot, row, productSize);
}

bool ShopSession::arrive(bool &changed)
{
    changed = false;
    if (!art) return true;
    const uint8_t species = pet.speciesSlot();
    const uint8_t mask = 1U << (species & 7U);
    auto &state = pet.st;
    const bool fresh = state.shopFingerprint != fingerprint;
    if (!fresh && (state.shopVisited[species / 8] & mask)) return true;

    ShopFile file;
    uint16_t quantities[64] = {};
    if (!file.open(path, FILE_READ))
    {
        resources.recordAssetDataErrorResource(path);
        return false;
    }
    for (uint8_t index = 0; index < itemCount; ++index)
    {
        uint8_t row[9];
        if (!read(file, itemOffset(index), row, sizeof(row))) return false;
        const uint16_t capacity = readU16(row + 2);
        uint16_t quantity = fresh ? readU16(row) : state.shopQuantities[index];
        if (row[7] == species && quantity < capacity) ++quantity;
        quantities[index] = quantity;
    }
    if (fresh) memset(state.shopVisited, 0, sizeof(state.shopVisited));
    memcpy(state.shopQuantities, quantities, sizeof(quantities));
    state.shopFingerprint = fingerprint;
    state.shopVisited[species / 8] |= mask;
    changed = true;
    return true;
}

bool ShopSession::unlocked(const uint8_t *row) const
{
    const uint8_t species = row[7];
    if (species && !(pet.st.shopVisited[species / 8] & (1U << (species & 7U))))
        return false;
    const uint8_t *condition = row + 10;
    for (uint8_t index = 0; index < row[9]; ++index, condition += 5)
        if (pet.customStat(condition[0]) < readU32(condition + 1))
            return false;
    return true;
}

bool ShopSession::buildEntries()
{
    count = 0;
    if (inventory)
    {
        for (uint8_t index = 0; index < itemCount; ++index)
            if (pet.st.shopQuantities[index]) entries[count++] = index;
    }
    else if (productCount)
    {
        ShopFile file;
        if (!file.open(path, FILE_READ))
        {
            resources.recordAssetDataErrorResource(path);
            return false;
        }
        for (uint8_t index = 0; index < productCount; ++index)
        {
            uint8_t row[productSize];
            if (!product(file, index, row)) return false;
            if (!row[8] || unlocked(row)) entries[count++] = index;
        }
    }
    entries[count++] = navigation;
    entries[count++] = exitEntry;
    if (cursor >= count) cursor = 0;
    return true;
}

bool ShopSession::start(bool showInventory)
{
    if (!art) return false;
    inventory = showInventory;
    cursor = 0;
    view = View::Browse;
    return buildEntries() && redraw();
}

bool ShopSession::move(bool next)
{
    if (view == View::Confirm) cancel = !cancel;
    else if (view == View::Browse) cursor = (cursor + (next ? 1 : count - 1)) % count;
    else return true;
    return redraw();
}

bool ShopSession::confirm(AssetData::AnimationRef &animation)
{
    if (view == View::Receipt)
    {
        view = View::Browse;
        return buildEntries() && redraw();
    }
    if (view == View::Confirm)
    {
        if (cancel) view = View::Browse;
        else if (!transact(animation)) return false;
        return animation.valid() || redraw();
    }
    const uint8_t entry = entries[cursor];
    if (entry == exitEntry)
    {
        close();
        return true;
    }
    if (entry == navigation) return start(!inventory);
    view = View::Confirm;
    cancel = true;
    return redraw();
}

bool ShopSession::transact(AssetData::AnimationRef &animation)
{
    ShopFile file;
    if (!file.open(path, FILE_READ))
    {
        resources.recordAssetDataErrorResource(path);
        return false;
    }
    uint8_t slot = entries[cursor];
    uint8_t row[itemSize];
    uint8_t offer[productSize];
    if (!inventory)
    {
        if (!product(file, slot, offer)) return false;
        slot = offer[6];
    }
    if (!item(file, slot, row)) return false;
    const uint16_t owned = pet.st.shopQuantities[slot];
    const uint16_t capacity = readU16(row + 2);
    const bool permanent = row[6] != 0;
    view = View::Receipt;
    PetStatSnapshot values = pet.statSnapshot();
    uint16_t nextQuantity;
    if (inventory)
    {
        const uint16_t consumed = readU16(row + 4);
        if (permanent) { receipt = Owned; return true; }
        if (owned < consumed) { receipt = Quantity; return true; }
        nextQuantity = owned - consumed;
        const uint8_t *effect = row + 9;
        for (uint8_t index = 0; index < row[8]; ++index, effect += 10)
        {
            const uint8_t stat = effect[0];
            const auto &bounds = config.stats[stat];
            int64_t value = readI64(effect + 2);
            if (!effect[1]) value += values.customStats[stat];
            if (value < bounds.minValue) value = bounds.minValue;
            if (value > bounds.maxValue) value = bounds.maxValue;
            values.customStats[stat] = static_cast<uint32_t>(value);
        }
        uint8_t binding[2];
        if (!read(file, itemOffset(slot) + itemSize + 2 * (pet.speciesSlot() - 1U),
                  binding, sizeof(binding))) return false;
        animation = {pet.speciesSlot(), pet.outfitSlot(), readU16(binding)};
        receipt = Used;
    }
    else
    {
        const uint32_t price = readU32(offer);
        const uint16_t added = readU16(offer + 4);
        if (!unlocked(offer)) { receipt = Locked; return true; }
        if (permanent && owned) { receipt = Owned; return true; }
        if (static_cast<uint32_t>(owned) + added > capacity) { receipt = Full; return true; }
        const uint32_t balance = values.customStats[wallet];
        if (balance < config.stats[wallet].minValue ||
            balance - config.stats[wallet].minValue < price) { receipt = Funds; return true; }
        values.customStats[wallet] -= price;
        nextQuantity = owned + added;
        receipt = Bought;
    }
    file.close();

    const PersistedPetState previous = pet.st;
    pet.st.shopQuantities[slot] = nextQuantity;
    if (config.statCount) pet.commitPetStats(values.customStats, config.statCount);
    if ((!pet.isDead() && !appearance.refreshUnlockState(false, false)) ||
        saves.saveCollectionProgress(true, true) != PetSaveResult::Persisted)
    {
        pet.restoreState(previous);
        animation = {};
        receipt = SaveFailed;
    }
    return true;
}

bool ShopSession::show(uint16_t frame, Rect rect)
{
    return renderer.ShowDataFrame({0, 0, art, 0, frame}, rect.x, rect.y, 12, rect.w, rect.h);
}

void ShopSession::number(Rect rect, uint32_t value)
{
    renderer.drawShopNumber(rect.x, rect.y, rect.w, rect.h, value);
}

bool ShopSession::redraw()
{
    const auto frameFor = [this](uint8_t entry) {
        const uint16_t slot = entry >= navigation
            ? itemCount + productCount + (entry == exitEntry ? 2 : inventory ? 1 : 0)
            : (inventory ? entry : itemCount + entry);
        return static_cast<uint16_t>(firstEntryFrame + 2 * slot);
    };
    const uint8_t selected = entries[cursor];
    const uint16_t current = frameFor(selected);
    if (!show(rowHeight ? 0 : current, {0, 0, 128, 160})) return false;
    if (rowHeight)
    {
        const uint8_t rows = listRect.h / rowHeight;
        const uint8_t first = cursor < rows ? 0 : cursor - rows + 1;
        for (uint8_t index = first; index < count && index - first < rows; ++index)
        {
            Rect rect = listRect;
            rect.y += (index - first) * rowHeight;
            rect.h = rowHeight;
            if (!show(frameFor(entries[index]), rect)) return false;
            if (index == cursor) renderer.highlightShopRow(rect.x, rect.y, rect.w, rect.h);
        }
    }
    if (walletRect.w) number(walletRect, pet.customStat(wallet));
    if (!rowHeight && quantityRect.w && selected < navigation)
    {
        uint8_t slot = selected;
        if (!inventory)
        {
            ShopFile file;
            uint8_t row[productSize];
            if (!file.open(path, FILE_READ) || !product(file, selected, row))
            {
                resources.recordAssetDataErrorResource(path);
                return false;
            }
            slot = row[6];
        }
        number(quantityRect, pet.st.shopQuantities[slot]);
    }
    if (view == View::Confirm)
    {
        ShopFile file;
        uint8_t row[itemSize];
        if (!file.open(path, FILE_READ) ||
            !(inventory ? item(file, selected, row) : product(file, selected, row)))
        {
            resources.recordAssetDataErrorResource(path);
            return false;
        }
        if (!show(current + 1, {0, 88, 128, 16}) ||
            !show(inventory ? 15 : 14, {0, 104, 128, 16})) return false;
        number({0, 120, 60, 16}, inventory ? pet.st.shopQuantities[selected] : readU32(row));
        number({68, 120, 60, 16}, readU16(row + 4));
    }
    else if (selected >= navigation && !rowHeight && !show(current + 1, {0, 112, 128, 16}))
        return false;

    Footer footer = Browse;
    if (view == View::Confirm)
        footer = inventory ? (cancel ? CancelUse : Use) : (cancel ? CancelBuy : Buy);
    if (view == View::Receipt) footer = receipt;
    return show(footer, {0, 144, 128, 16});
}
#endif
