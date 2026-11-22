#include "shop/ShopSession.h"
#if ENABLE_SHOP

#include <string.h>
#include "appearance/AppearanceChangeController.h"
#include "common/SdBinaryRead.h"
#include "collection/CollectionGameEvent.h"
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
constexpr uint8_t stageDaysEffectSlot = 255;
constexpr uint16_t selectedRow = 0x8000;
constexpr DisplayRect screen{0, 0, 128, 160};
constexpr DisplayRect confirmation{0, 88, 128, 48};
constexpr DisplayRect navigationCard{0, 112, 128, 16};

DisplayRect combine(DisplayRect a, DisplayRect b)
{
    if (!a.w) return b;
    if (!b.w) return a;
    const uint8_t x = a.x < b.x ? a.x : b.x;
    const uint8_t y = a.y < b.y ? a.y : b.y;
    const int right = a.right() > b.right() ? a.right() : b.right();
    const int bottom = a.bottom() > b.bottom() ? a.bottom() : b.bottom();
    return {x, y, static_cast<uint8_t>(right - x), static_cast<uint8_t>(bottom - y)};
}

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
    // The optional trailer leaves the original Shop records at their old offsets.
    const uint32_t recordsEnd = itemOffset(itemCount) + static_cast<uint32_t>(productSize) * productCount;
    boundsOffset = !rowHeight && file.fileSize() > recordsEnd ? recordsEnd : 0;
    const uint32_t boundsEnd = recordsEnd + (rowHeight ? 0 : sizeof(Rect) * (itemCount + productCount + 3));
    confirmationOffset = 0;
    uint8_t signature[4];
    if (file.fileSize() >= boundsEnd + sizeof(signature))
    {
        if (!read(file, boundsEnd, signature, sizeof(signature))) return false;
        if (memcmp(signature, "SCF1", sizeof(signature)) == 0) confirmationOffset = boundsEnd;
    }
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

ShopConfirmationNotice ShopSession::unlockNotice(const uint8_t *row) const
{
    const uint8_t species = row[7];
    if (species && !(pet.st.shopVisited[species / 8] & (1U << (species & 7U))))
        return ShopConfirmationNotice::Species;
    const uint8_t *condition = row + 10;
    for (uint8_t index = 0; index < row[9]; ++index, condition += 5)
        if (pet.customStat(condition[0]) < readU32(condition + 1))
            return ShopConfirmationNotice::Condition;
    return ShopConfirmationNotice::None;
}

ShopConfirmationNotice ShopSession::purchaseNotice(const uint8_t *offer, const uint8_t *row,
                                                    uint16_t owned, uint32_t balance) const
{
    const ShopConfirmationNotice locked = unlockNotice(offer);
    if (locked != ShopConfirmationNotice::None) return locked;
    if (row[6] && owned) return ShopConfirmationNotice::Owned;
    if (static_cast<uint32_t>(owned) + readU16(offer + 4) > readU16(row + 2))
        return ShopConfirmationNotice::Capacity;
    const uint32_t minimum = config.stats[wallet].minValue;
    if (balance < minimum || balance - minimum < readU32(offer)) return ShopConfirmationNotice::Funds;
    return ShopConfirmationNotice::None;
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
            if (!row[8] || unlockNotice(row) == ShopConfirmationNotice::None) entries[count++] = index;
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
    if (view == View::Confirm)
    {
        if (cancel == next) return true;
        cancel = next;
        return confirmationOffset ? paintConfiguredConfirmation(entries[cursor], true) : paintFooter();
    }
    if (view != View::Browse) return true;
    cursor = (cursor + (next ? 1 : count - 1)) % count;
    return paint();
}

bool ShopSession::confirm(AssetData::AnimationRef &animation)
{
    if (view == View::Receipt)
    {
        view = View::Browse;
        return buildEntries() && paint();
    }
    if (view == View::Confirm)
    {
        if (cancel) view = View::Browse;
        else if (!transact(animation)) return false;
        return animation.valid() || paint();
    }
    const uint8_t entry = entries[cursor];
    if (entry == exitEntry)
    {
        close();
        return true;
    }
    if (entry == navigation)
    {
        inventory = !inventory;
        cursor = 0;
        return buildEntries() && paint();
    }
    view = View::Confirm;
    cancel = true;
    return paint();
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
            if (stat == stageDaysEffectSlot)
            {
                int64_t days = readI64(effect + 2);
                if (!effect[1]) days += values.stage_days;
                values.stage_days = days < 0 ? 0 : days > Pet::kStageDaysMax ? Pet::kStageDaysMax : days;
                continue;
            }
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
        const ShopConfirmationNotice notice = purchaseNotice(offer, row, owned, values.customStats[wallet]);
        if (notice != ShopConfirmationNotice::None)
        {
            constexpr Footer notices[]{Browse, Locked, Locked, Owned, Full, Funds};
            receipt = notices[static_cast<uint8_t>(notice)];
            return true;
        }
        values.customStats[wallet] -= price;
        nextQuantity = owned + added;
        receipt = Bought;
    }
    file.close();

    const PersistedPetState previous = pet.st;
    pet.st.shopQuantities[slot] = nextQuantity;
    pet.setStageDays(values.stage_days);
    if (config.statCount) pet.commitPetStats(values.customStats, config.statCount);
    if (
#if ENABLE_COLLECTION_GAME_EVENTS
        !recordCollectionGameEvent(resources.sdCard(), config, pet, previous,
                                  CollectionGameEvent::StatTransition, inventory ? slot : 255) ||
#endif
        (!pet.isDead() && !appearance.refreshUnlockState(false, false)) ||
        saves.saveCollectionProgress(true, true) != PetSaveResult::Persisted)
    {
        pet.restoreState(previous);
        animation = {};
        receipt = SaveFailed;
    }
    return true;
}

bool ShopSession::show(uint16_t frame, Rect rect, const Rect *crop)
{
    return renderer.ShowDataFrame({0, 0, art, 0, frame}, rect.x, rect.y, 12, rect.w, rect.h, crop);
}

void ShopSession::number(Rect rect, uint32_t value)
{
    renderer.drawShopNumber(rect.x, rect.y, rect.w, rect.h, value);
}

uint16_t ShopSession::frameFor(uint8_t entry) const
{
    const uint16_t slot = entry >= navigation
        ? itemCount + productCount + (entry == exitEntry ? 2 : inventory ? 1 : 0)
        : (inventory ? entry : itemCount + entry);
    return firstEntryFrame + 2 * slot;
}

bool ShopSession::redraw()
{
    return paint(true);
}

bool ShopSession::paint(bool full)
{
    const uint8_t selected = entries[cursor];
    if (confirmationOffset && view == View::Confirm)
    {
        if (!paintConfiguredConfirmation(selected)) return false;
        paintedView = View::Confirm;
        return true;
    }
    // Authored confirmation blocks can cover any part of the browse scene.
    full |= confirmationOffset && paintedView == View::Confirm;
    const uint16_t current = frameFor(selected);
    const bool pageChanged = full || current != paintedFrame;
    const bool restoring = paintedView == View::Confirm && view != View::Confirm;
    const bool hasQuantity = !rowHeight && quantityRect.w && selected < navigation;
    Rect dirty = restoring ? confirmation : Rect{};
    if (!rowHeight && pageChanged)
    {
        Rect nextBounds{0, 0, 128, 144}; // Earlier exports have no trailer.
        if (boundsOffset)
        {
            ShopFile file;
            const uint32_t offset = boundsOffset + sizeof(Rect) * ((current - firstEntryFrame) / 2);
            if (!file.open(path, FILE_READ))
            {
                resources.recordAssetDataErrorResource(path);
                return false;
            }
            if (!read(file, offset, reinterpret_cast<uint8_t *>(&nextBounds), sizeof(nextBounds))) return false;
        }
        dirty = combine(dirty, combine(paintedBounds, nextBounds));
        paintedBounds = nextBounds;
        if (paintedHasQuantity && !hasQuantity) dirty = combine(dirty, quantityRect);
    }
    const uint16_t base = rowHeight ? 0 : current;
    if (full ? !show(base, screen) : dirty.w && !show(base, screen, &dirty)) return false;

    const uint32_t balance = walletRect.w ? pet.customStat(wallet) : 0;
    bool walletDirty = full || balance != paintedWallet || dirty.overlaps(walletRect);
    const uint16_t firstNavigationFrame = firstEntryFrame + 2 * (itemCount + productCount);
    walletDirty |= !rowHeight && pageChanged && paintedFrame >= firstNavigationFrame &&
                   navigationCard.overlaps(walletRect);
    if (rowHeight)
    {
        const uint8_t rows = listRect.h / rowHeight;
        const uint8_t first = cursor < rows ? 0 : cursor - rows + 1;
        for (uint8_t row = 0; row < rows; ++row)
        {
            const uint8_t index = first + row;
            const uint16_t frame = index < count ? frameFor(entries[index]) : 0;
            const uint16_t key = frame | (index == cursor ? selectedRow : 0);
            const Rect rect{listRect.x, static_cast<uint8_t>(listRect.y + row * rowHeight), listRect.w, rowHeight};
            if (!full && key == paintedRows[row] && (!frame || !dirty.overlaps(rect))) continue;
            if (frame)
            {
                if (!show(frame, rect)) return false;
                if (index == cursor) renderer.highlightShopRow(rect.x, rect.y, rect.w, rect.h);
            }
            else if (!full && !show(0, screen, &rect)) return false;
            paintedRows[row] = key;
            walletDirty |= rect.overlaps(walletRect);
        }
    }
    if (walletDirty) number(walletRect, balance);
    if (hasQuantity)
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
        const uint16_t quantity = pet.st.shopQuantities[slot];
        if (full || !paintedHasQuantity || quantity != paintedQuantity || dirty.overlaps(quantityRect) ||
            (walletDirty && walletRect.overlaps(quantityRect))) number(quantityRect, quantity);
        paintedQuantity = quantity;
    }
    if (view == View::Confirm)
    {
        if (!paintConfirmation(current, selected)) return false;
    }
    else if (selected >= navigation && !rowHeight &&
             (pageChanged || (walletDirty && walletRect.overlaps(navigationCard))) &&
             !show(current + 1, navigationCard))
        return false;

    paintedFrame = current;
    paintedView = view;
    paintedWallet = balance;
    paintedHasQuantity = hasQuantity;
    return paintFooter(full);
}

bool ShopSession::paintConfirmation(uint16_t frame, uint8_t selected)
{
    ShopFile file;
    uint8_t row[itemSize];
    if (!file.open(path, FILE_READ) ||
        !(inventory ? item(file, selected, row) : product(file, selected, row)))
    {
        resources.recordAssetDataErrorResource(path);
        return false;
    }
    if (!show(frame + 1, {0, 88, 128, 16}) ||
        !show(inventory ? 15 : 14, {0, 104, 128, 16})) return false;
    number({0, 120, 60, 16}, inventory ? pet.st.shopQuantities[selected] : readU32(row));
    number({68, 120, 60, 16}, readU16(row + 4));
    return true;
}

bool ShopSession::paintConfiguredConfirmation(uint8_t selected, bool buttonsOnly)
{
    const uint16_t background = firstEntryFrame + 2 * (itemCount + productCount + 3);
    ShopFile file;
    if (!file.open(path, FILE_READ))
    {
        resources.recordAssetDataErrorResource(path);
        return false;
    }

    ShopConfirmationValues values{};
    ShopConfirmationNotice notice = ShopConfirmationNotice::None;
    if (inventory)
        values.owned = pet.st.shopQuantities[selected];
    else
    {
        uint8_t offer[productSize], row[itemSize];
        if (!product(file, selected, offer) || !item(file, offer[6], row)) return false;
        values = {readU32(offer), readU16(offer + 4), pet.st.shopQuantities[offer[6]]};
        notice = purchaseNotice(offer, row, values.owned, pet.customStat(wallet));
    }

    const uint16_t entry = inventory ? selected : itemCount + selected;
    if (drawShopConfirmation(file, confirmationOffset, entry, cancel, values, notice, renderer,
                             {0, 0, art, 0, background}, buttonsOnly)) return true;
    resources.recordAssetDataErrorResource(path);
    return false;
}

bool ShopSession::paintFooter(bool full)
{
    Footer footer = Browse;
    if (view == View::Confirm)
        footer = inventory ? (cancel ? CancelUse : Use) : (cancel ? CancelBuy : Buy);
    if (view == View::Receipt) footer = receipt;
    if (!full && footer == paintedFooter) return true;
    paintedFooter = footer;
    return show(footer, {0, 144, 128, 16});
}
#endif
