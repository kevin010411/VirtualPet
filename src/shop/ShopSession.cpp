#include "shop/ShopSession.h"
#if ENABLE_SHOP

#include <string.h>
#include "appearance/AppearanceChangeController.h"
#include "collection/CollectionGameEvent.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorTypes.h"
#include "pet/PetSaveController.h"
#include "resources/AssetResources.h"
#include "resources/detail/RuntimeTableReader.h"
#include "shop/ShopRules.h"

using RuntimeTableInternal::readU16;
using RuntimeTableInternal::readU32;
using RuntimeTableInternal::readI64;

namespace
{
constexpr uint8_t stageDaysEffectSlot = 255;
}

ShopSession::ShopSession(Pet &pet, PetSaveController &saves, AppearanceChangeController &appearance,
                         Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config
#if ENABLE_COLLECTION_GAME_EVENTS
                         , const RuntimeTableConfig &collectionSource
#endif
                         )
    : pet(pet), saves(saves), appearance(appearance),
#if ENABLE_COLLECTION_GAME_EVENTS
      collectionSource(collectionSource),
#endif
      config(config), data(resources), display(renderer)
{
}

bool ShopSession::configure(bool enabled, const AssetData::BundleId &bundleId)
{
    return data.configure(enabled, bundleId);
}

bool ShopSession::redraw()
{
    return display.paint(data, selection, pet, config, true);
}

bool ShopSession::arrive(bool &changed)
{
    changed = false;
    if (!data.art) return true;
    const uint8_t species = pet.speciesSlot();
    const uint8_t mask = 1U << (species & 7U);
    auto &state = pet.st;
    const bool fresh = state.shopFingerprint != data.fingerprint;
    if (!fresh && (state.shopVisited[species / 8] & mask)) return true;

    ShopData::File file;
    uint16_t quantities[64] = {};
    if (!data.open(file)) return false;
    for (uint8_t index = 0; index < data.itemCount; ++index)
    {
        uint8_t row[9];
        if (!data.arrival(file, index, row)) return false;
        const uint16_t capacity = readU16(row + 2);
        uint16_t quantity = fresh ? readU16(row) : state.shopQuantities[index];
        if (row[7] == species && quantity < capacity) ++quantity;
        quantities[index] = quantity;
    }
    if (fresh) memset(state.shopVisited, 0, sizeof(state.shopVisited));
    memcpy(state.shopQuantities, quantities, sizeof(quantities));
    state.shopFingerprint = data.fingerprint;
    state.shopVisited[species / 8] |= mask;
    changed = true;
    return true;
}

ShopConfirmationNotice shopUnlockNotice(const Pet &pet, const uint8_t *row)
{
    const uint8_t species = row[7];
    if (species && !(pet.persistentState().shopVisited[species / 8] & (1U << (species & 7U))))
        return ShopConfirmationNotice::Species;
    const uint8_t *condition = row + 10;
    for (uint8_t index = 0; index < row[9]; ++index, condition += 5)
        if (pet.customStat(condition[0]) < readU32(condition + 1))
            return ShopConfirmationNotice::Condition;
    return ShopConfirmationNotice::None;
}

ShopConfirmationNotice shopPurchaseNotice(const Pet &pet, uint32_t minimumBalance,
                                          const uint8_t *offer, const uint8_t *row,
                                          uint16_t owned, uint32_t balance)
{
    const ShopConfirmationNotice locked = shopUnlockNotice(pet, offer);
    if (locked != ShopConfirmationNotice::None) return locked;
    if (row[6] && owned) return ShopConfirmationNotice::Owned;
    if (static_cast<uint32_t>(owned) + readU16(offer + 4) > readU16(row + 2))
        return ShopConfirmationNotice::Capacity;
    if (balance < minimumBalance || balance - minimumBalance < readU32(offer)) return ShopConfirmationNotice::Funds;
    return ShopConfirmationNotice::None;
}

bool ShopSession::buildEntries()
{
    selection.count = 0;
    if (selection.inventory)
    {
        for (uint8_t index = 0; index < data.itemCount; ++index)
            if (pet.st.shopQuantities[index]) selection.entries[selection.count++] = index;
    }
    else if (data.productCount)
    {
        ShopData::File file;
        if (!data.open(file)) return false;
        for (uint8_t index = 0; index < data.productCount; ++index)
        {
            uint8_t row[ShopData::productSize];
            if (!data.product(file, index, row)) return false;
            if (!row[8] || shopUnlockNotice(pet, row) == ShopConfirmationNotice::None) selection.entries[selection.count++] = index;
        }
    }
    selection.entries[selection.count++] = ShopSelection::navigation;
    selection.entries[selection.count++] = ShopSelection::exitEntry;
    if (selection.cursor >= selection.count) selection.cursor = 0;
    return true;
}

bool ShopSession::start(bool showInventory)
{
    if (!data.art) return false;
    selection.inventory = showInventory;
    selection.cursor = 0;
    selection.view = View::Browse;
    return buildEntries() && redraw();
}

bool ShopSession::move(bool next)
{
    if (selection.view == View::Confirm)
    {
        if (selection.cancel == next) return true;
        selection.cancel = next;
        return display.moveConfirmation(data, selection, pet, config);
    }
    if (selection.view != View::Browse) return true;
    selection.cursor = (selection.cursor + (next ? 1 : selection.count - 1)) % selection.count;
    return display.paint(data, selection, pet, config);
}

bool ShopSession::confirm(AssetData::AnimationRef &animation)
{
    if (selection.view == View::Receipt)
    {
        selection.view = View::Browse;
        return buildEntries() && display.paint(data, selection, pet, config);
    }
    if (selection.view == View::Confirm)
    {
        if (selection.cancel) selection.view = View::Browse;
        else if (!transact(animation)) return false;
        return animation.valid() || display.paint(data, selection, pet, config);
    }
    const uint8_t entry = selection.entries[selection.cursor];
    if (entry == ShopSelection::exitEntry)
    {
        close();
        return true;
    }
    if (entry == ShopSelection::navigation)
    {
        selection.inventory = !selection.inventory;
        selection.cursor = 0;
        return buildEntries() && display.paint(data, selection, pet, config);
    }
    selection.view = View::Confirm;
    selection.cancel = true;
    return display.paint(data, selection, pet, config);
}

bool ShopSession::transact(AssetData::AnimationRef &animation)
{
    ShopData::File file;
    if (!data.open(file)) return false;
    uint8_t slot = selection.entries[selection.cursor];
    uint8_t row[ShopData::itemSize];
    uint8_t offer[ShopData::productSize];
    if (!selection.inventory)
    {
        if (!data.product(file, slot, offer)) return false;
        slot = offer[6];
    }
    if (!data.item(file, slot, row)) return false;
    const uint16_t owned = pet.st.shopQuantities[slot];
    const bool permanent = row[6] != 0;
    selection.view = View::Receipt;
    PetStatSnapshot values = pet.statSnapshot();
    uint16_t nextQuantity;
    if (selection.inventory)
    {
        const uint16_t consumed = readU16(row + 4);
        if (permanent) { selection.receipt = Footer::Owned; return true; }
        if (owned < consumed) { selection.receipt = Footer::Quantity; return true; }
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
        uint16_t animationId;
        if (!data.useAnimation(file, slot, pet.speciesSlot(), animationId)) return false;
        animation = {pet.speciesSlot(), pet.outfitSlot(), animationId};
        selection.receipt = Footer::Used;
    }
    else
    {
        const uint32_t price = readU32(offer);
        const uint16_t added = readU16(offer + 4);
        const ShopConfirmationNotice notice = shopPurchaseNotice(
            pet, config.stats[data.wallet].minValue, offer, row, owned, values.customStats[data.wallet]);
        if (notice != ShopConfirmationNotice::None)
        {
            constexpr Footer notices[]{Footer::Browse, Footer::Locked, Footer::Locked, Footer::Owned, Footer::Full, Footer::Funds};
            selection.receipt = notices[static_cast<uint8_t>(notice)];
            return true;
        }
        values.customStats[data.wallet] -= price;
        nextQuantity = owned + added;
        selection.receipt = Footer::Bought;
    }
    file.close();

    const PersistedPetState previous = pet.st;
    pet.st.shopQuantities[slot] = nextQuantity;
    pet.setStageDays(values.stage_days);
    if (config.statCount) pet.commitPetStats(values.customStats, config.statCount);
    if (
#if ENABLE_COLLECTION_GAME_EVENTS
        !recordCollectionGameEvent(data.sdCard(), collectionSource, pet, previous,
                                  CollectionGameEvent::StatTransition, selection.inventory ? slot : 255) ||
#endif
        (!pet.isDead() && !appearance.refreshUnlockState(false, false)) ||
        saves.saveCollectionProgress(true, true) != PetSaveResult::Persisted)
    {
        pet.restoreState(previous);
        animation = {};
        selection.receipt = Footer::SaveFailed;
    }
    return true;
}

#endif
