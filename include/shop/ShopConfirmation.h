#pragma once

#include "common/AppProfile.h"
#if ENABLE_SHOP
#include <SdFat.h>
#include <stdint.h>

class Renderer;

enum class ShopConfirmationNotice : uint8_t { None, Species, Condition, Owned, Capacity, Funds };

struct ShopConfirmationValues
{
    uint32_t price;
    uint16_t quantity, owned;
};

struct ShopConfirmationBlock
{
    static constexpr uint8_t maskStride = 16;
    static constexpr uint8_t maxHeight = 26;
    using Mask = uint8_t[maxHeight][maskStride];

    int16_t x, y;
    uint8_t width, height, action;
    bool outlined;
    uint16_t fill, ink, selectedInk, outline, border;
};

bool drawShopConfirmation(SdBaseFile &file, uint32_t offset, uint16_t entry, bool returning,
                          ShopConfirmationValues values, ShopConfirmationNotice notice, Renderer &renderer);
#endif
