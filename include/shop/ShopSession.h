#pragma once

#include "common/AppProfile.h"
#if ENABLE_SHOP
#include "shop/ShopData.h"
#include "shop/ShopView.h"

class Pet;
class PetSaveController;
class AppearanceChangeController;
class AssetResources;
class Renderer;
struct PetBehaviorConfig;
struct RuntimeTableConfig;

class ShopSession
{
public:
    ShopSession(Pet &pet, PetSaveController &saves, AppearanceChangeController &appearance,
                Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config
#if ENABLE_COLLECTION_GAME_EVENTS
                , const RuntimeTableConfig &collectionSource
#endif
                );

    bool configure(bool enabled, const AssetData::BundleId &bundleId);
    bool arrive(bool &changed);
    bool start(bool inventory);
    bool move(bool next);
    bool confirm(AssetData::AnimationRef &animation);
    bool redraw();
    bool active() const { return selection.view != View::Closed; }
    void close() { selection.view = View::Closed; }

private:
    using View = ShopSelection::View;
    using Footer = ShopSelection::Footer;

    Pet &pet;
    PetSaveController &saves;
    AppearanceChangeController &appearance;
#if ENABLE_COLLECTION_GAME_EVENTS
    const RuntimeTableConfig &collectionSource;
#endif
    const PetBehaviorConfig &config;
    ShopData data;
    ShopSelection selection;
    ShopView display;

    bool buildEntries();
    bool transact(AssetData::AnimationRef &animation);
};
#endif
