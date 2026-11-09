#pragma once

#include "common/AppProfile.h"
#if ENABLE_SHOP
#include "resources/AssetRuntimeContract.h"

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
    struct Rect { uint8_t x, y, w, h; };

    Pet &pet;
    PetSaveController &saves;
    AppearanceChangeController &appearance;
    Renderer &renderer;
    AssetResources &resources;
    const PetBehaviorConfig &config;
    AssetData::BundleId bundle = {};
    uint32_t fingerprint = 0;
    uint16_t art = 0;
    uint8_t wallet = 255, itemCount = 0, productCount = 0, speciesCount = 0, rowHeight = 0;
    Rect walletRect = {}, quantityRect = {}, listRect = {};
    uint8_t entries[66] = {};
    uint8_t count = 0, cursor = 0;
    bool inventory = false;
    bool cancel = true;
    View view = View::Closed;
    Footer receipt = Browse;

    bool read(SdBaseFile &file, uint32_t offset, uint8_t *row, size_t length);
    uint32_t itemOffset(uint8_t slot) const;
    bool item(SdBaseFile &file, uint8_t slot, uint8_t *row);
    bool product(SdBaseFile &file, uint8_t slot, uint8_t *row);
    bool unlocked(const uint8_t *product) const;
    bool buildEntries();
    bool transact(AssetData::AnimationRef &animation);
    bool show(uint16_t frame, Rect rect);
    void number(Rect rect, uint32_t value);
};
#endif
