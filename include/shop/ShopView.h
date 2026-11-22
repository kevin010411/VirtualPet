#pragma once

#include "display/DisplayRect.h"

class Pet;
class ShopData;
class Renderer;
struct PetBehaviorConfig;

struct ShopSelection
{
    enum class View : uint8_t { Closed, Browse, Confirm, Receipt };
    enum Footer : uint8_t {
        Browse = 1, CancelBuy, Buy, CancelUse, Use, Bought, Used, Locked,
        Funds, Full, Owned, Quantity, SaveFailed
    };
    static constexpr uint8_t navigation = 128;
    static constexpr uint8_t exitEntry = 129;

    uint8_t entries[66] = {};
    uint8_t count = 0, cursor = 0;
    bool inventory = false;
    bool cancel = true;
    View view = View::Closed;
    Footer receipt = Browse;
};

class ShopView
{
public:
    explicit ShopView(Renderer &renderer) : renderer(renderer) {}
    bool paint(const ShopData &data, const ShopSelection &selection,
               const Pet &pet, const PetBehaviorConfig &config, bool full = false);
    bool moveConfirmation(const ShopData &data, const ShopSelection &selection,
                          const Pet &pet, const PetBehaviorConfig &config);

private:
    class Painter;
    Renderer &renderer;
    using View = ShopSelection::View;
    using Footer = ShopSelection::Footer;
    using Rect = DisplayRect;
    uint16_t paintedFrame = 0;
    uint16_t paintedRows[8 * 4] = {};
    Rect paintedBounds = {};
    uint32_t paintedWallet = 0;
    uint16_t paintedQuantity = 0;
    bool paintedHasQuantity = false;
    View paintedView = View::Closed;
    Footer paintedFooter = Footer::Browse;
};
