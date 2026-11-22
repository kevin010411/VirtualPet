#pragma once

#include "common/AppProfile.h"
#if ENABLE_SHOP
#include "display/DisplayRect.h"
#include "resources/AssetRuntimeContract.h"
#include "shop/ShopConfirmation.h"

class Pet;
class PetSaveController;
class AppearanceChangeController;
class AssetResources;
class Renderer;
struct PetBehaviorConfig;

class ShopSession
{
public:
    ShopSession(Pet &pet, PetSaveController &saves, AppearanceChangeController &appearance,
                Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config);

    bool configure();
    bool arrive(bool &changed);
    bool start(bool inventory);
    bool move(bool next);
    bool confirm(AssetData::AnimationRef &animation);
    bool redraw();
    bool active() const { return view != View::Closed; }
    void close() { view = View::Closed; }

private:
    enum class View : uint8_t { Closed, Browse, Confirm, Receipt };
    enum Footer : uint8_t {
        Browse = 1, CancelBuy, Buy, CancelUse, Use, Bought, Used, Locked,
        Funds, Full, Owned, Quantity, SaveFailed
    };
    using Rect = DisplayRect;

    Pet &pet;
    PetSaveController &saves;
    AppearanceChangeController &appearance;
    Renderer &renderer;
    AssetResources &resources;
    const PetBehaviorConfig &config;
    AssetData::BundleId bundle = {};
    uint32_t fingerprint = 0;
    uint32_t boundsOffset = 0;
    uint32_t confirmationOffset = 0;
    uint16_t art = 0;
    uint8_t wallet = 255, itemCount = 0, productCount = 0, speciesCount = 0, rowHeight = 0;
    Rect walletRect = {}, quantityRect = {}, listRect = {};
    uint8_t entries[66] = {};
    uint8_t count = 0, cursor = 0;
    bool inventory = false;
    bool cancel = true;
    View view = View::Closed;
    Footer receipt = Browse;
    uint16_t paintedFrame = 0;
    uint16_t paintedRows[8] = {};
    Rect paintedBounds = {};
    uint32_t paintedWallet = 0;
    uint16_t paintedQuantity = 0;
    bool paintedHasQuantity = false;
    View paintedView = View::Closed;
    Footer paintedFooter = Browse;

    bool read(SdBaseFile &file, uint32_t offset, uint8_t *row, size_t length);
    uint32_t itemOffset(uint8_t slot) const;
    bool item(SdBaseFile &file, uint8_t slot, uint8_t *row);
    bool product(SdBaseFile &file, uint8_t slot, uint8_t *row);
    ShopConfirmationNotice unlockNotice(const uint8_t *product) const;
    ShopConfirmationNotice purchaseNotice(const uint8_t *offer, const uint8_t *item,
                                          uint16_t owned, uint32_t balance) const;
    bool buildEntries();
    bool transact(AssetData::AnimationRef &animation);
    uint16_t frameFor(uint8_t entry) const;
    bool paint(bool full = false);
    bool paintFooter(bool full = false);
    bool paintConfirmation(uint16_t frame, uint8_t selected);
    bool paintConfiguredConfirmation(uint8_t selected, bool buttonsOnly = false);
    bool show(uint16_t frame, Rect rect, const Rect *crop = nullptr);
    void number(Rect rect, uint32_t value);
};
#endif
