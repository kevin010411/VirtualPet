#include "common/AppProfile.h"
#if ENABLE_SHOP
#include "shop/ShopView.h"

#include "display/NumberGlyphs.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorTypes.h"
#include "resources/detail/RuntimeTableReader.h"
#include "shop/ShopData.h"
#include "shop/ShopRules.h"

using RuntimeTableInternal::readU16;
using RuntimeTableInternal::readU32;

namespace
{
constexpr uint8_t firstEntryFrame = 16;
constexpr uint8_t bandHeight = 16;
constexpr uint8_t receiptCount = 8;
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

void drawNumber(Renderer &renderer, DisplayRect rect, uint32_t value)
{
    const auto [x, y, width, height] = rect;
    auto &display = renderer.pixelDisplay();
    if (!width) return;
    uint8_t digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = value % 10;
        value /= 10;
    } while (value);
    int16_t left = width - count * 6;
    if (count * 6 > width)
    {
        digits[0] = 10;
        count = 1;
        left = 0;
    }
    const int16_t top = height / 2 - 4;
    uint16_t *line = renderer.pixelWorkspace();
    display.startWrite();
    display.setAddrWindow(x, y, width, height);
    for (uint8_t row = 0; row < height; ++row)
    {
        for (uint8_t col = 0; col < width; ++col) line[col] = 0x1126;
        const int16_t glyphRow = row - top;
        if (glyphRow >= 0 && glyphRow < 8)
        {
            int16_t pen = left;
            for (uint8_t digit = count; digit > 0; --digit, pen += 6)
            {
                const uint8_t bits = NumberGlyphs::digits[digits[digit - 1]][glyphRow];
                for (uint8_t col = 0; col < 5 && pen + col < width; ++col)
                    if (bits & (0x80 >> col)) line[pen + col] = 0xffff;
            }
        }
        display.writePixels(line, width);
    }
    display.endWrite();
}

}

class ShopView::Painter
{
public:
    Painter(ShopView &cache, const ShopData &data, const ShopSelection &selection,
            const Pet &pet, const PetBehaviorConfig &config)
        : cache(cache), data(data), selection(selection), pet(pet), config(config), renderer(cache.renderer) {}

    bool paint(bool full);
    bool paintFooter(bool full = false);
    bool paintConfiguredConfirmation(uint8_t selected, bool buttonsOnly = false);

private:
    ShopView &cache;
    const ShopData &data;
    const ShopSelection &selection;
    const Pet &pet;
    const PetBehaviorConfig &config;
    Renderer &renderer;

    uint16_t frameFor(uint8_t entry) const;
    uint16_t confirmationFrame() const;
    bool paintConfirmation(uint16_t frame, uint8_t selected);
    bool show(uint16_t frame, Rect rect, const Rect *crop = nullptr);
    bool restoreBrowse(uint16_t frame, Rect dirty);
    void number(Rect rect, uint32_t value);
};

bool ShopView::paint(const ShopData &data, const ShopSelection &selection, const Pet &pet,
                     const PetBehaviorConfig &config, bool full)
{
    return Painter(*this, data, selection, pet, config).paint(full);
}

bool ShopView::moveConfirmation(const ShopData &data, const ShopSelection &selection, const Pet &pet,
                                const PetBehaviorConfig &config)
{
    Painter painter(*this, data, selection, pet, config);
    if (data.hasConfirmation())
        return painter.paintConfiguredConfirmation(selection.entries[selection.cursor], true);
    return painter.paintFooter();
}

bool ShopView::Painter::show(uint16_t frame, Rect rect, const Rect *crop)
{
    return renderer.ShowDataFrame({0, 0, data.art, 0, frame}, rect.x, rect.y, 12, rect.w, rect.h, crop);
}

bool ShopView::Painter::restoreBrowse(uint16_t frame, Rect dirty)
{
    if (!data.hasBrowseBands()) return show(frame, screen, &dirty);
    const uint16_t page = frame ? 1 + (frame - firstEntryFrame) / 2 : 0;
    const uint8_t first = dirty.y / bandHeight;
    const uint8_t end = (dirty.bottom() + bandHeight - 1) / bandHeight;
    uint8_t addresses[2 * ShopData::bandCount];
    if (!data.browseBands(page, first, end - first, addresses)) return false;
    for (uint8_t band = first; band < end; ++band)
    {
        const uint8_t y = band * bandHeight;
        const int top = dirty.y > y ? dirty.y : y;
        const int bottom = dirty.bottom() < y + bandHeight ? dirty.bottom() : y + bandHeight;
        const Rect crop{dirty.x, static_cast<uint8_t>(top - y), dirty.w, static_cast<uint8_t>(bottom - top)};
        if (!show(readU16(addresses + 2 * (band - first)), {0, y, 128, bandHeight}, &crop)) return false;
    }
    return true;
}

void ShopView::Painter::number(Rect rect, uint32_t value)
{
    drawNumber(renderer, rect, value);
}

uint16_t ShopView::Painter::frameFor(uint8_t entry) const
{
    const uint16_t slot = entry >= ShopSelection::navigation
        ? data.itemCount + data.productCount + (entry == ShopSelection::exitEntry ? 2 : selection.inventory ? 1 : 0)
        : (selection.inventory ? entry : data.itemCount + entry);
    return firstEntryFrame + 2 * slot;
}

uint16_t ShopView::Painter::confirmationFrame() const
{
    return firstEntryFrame + 2 * (data.itemCount + data.productCount + 3);
}

bool ShopView::Painter::paint(bool full)
{
    const uint8_t selected = selection.entries[selection.cursor];
    if (data.hasConfirmation() && selection.view == View::Confirm)
    {
        if (!paintConfiguredConfirmation(selected)) return false;
        cache.paintedView = View::Confirm;
        return true;
    }
    if (data.styledBrowse && selection.view == View::Receipt)
    {
        if (!show(confirmationFrame() + 1 + selection.receipt - Footer::Bought, screen)) return false;
        cache.paintedView = View::Receipt;
        return true;
    }
    // Authored confirmation blocks can cover any part of the browse scene.
    full |= (data.hasConfirmation() && cache.paintedView == View::Confirm) || (data.styledBrowse && cache.paintedView == View::Receipt);
    const uint16_t current = frameFor(selected);
    const bool pageChanged = full || current != cache.paintedFrame;
    const bool restoring = cache.paintedView == View::Confirm && selection.view != View::Confirm;
    const bool hasQuantity = !data.rowHeight && data.quantityRect.w && selected < ShopSelection::navigation;
    Rect dirty = restoring ? confirmation : Rect{};
    if (!data.rowHeight && pageChanged)
    {
        Rect nextBounds{0, 0, 128, 144}; // Earlier exports have no trailer.
        if (!data.pageBounds((current - firstEntryFrame) / 2, nextBounds)) return false;
        dirty = combine(dirty, combine(cache.paintedBounds, nextBounds));
        cache.paintedBounds = nextBounds;
        if (cache.paintedHasQuantity && !hasQuantity) dirty = combine(dirty, data.quantityRect);
    }
    const uint16_t base = data.rowHeight ? 0 : current;
    if (full ? !show(base, screen) : dirty.w && !restoreBrowse(base, dirty)) return false;

    const uint32_t balance = data.walletRect.w ? pet.customStat(data.wallet) : 0;
    bool walletDirty = full || balance != cache.paintedWallet || dirty.overlaps(data.walletRect);
    const uint16_t firstNavigationFrame = firstEntryFrame + 2 * (data.itemCount + data.productCount);
    walletDirty |= !data.styledBrowse && !data.rowHeight && pageChanged && cache.paintedFrame >= firstNavigationFrame &&
                   navigationCard.overlaps(data.walletRect);
    if (data.rowHeight)
    {
        const uint8_t rows = data.listRect.h / data.rowHeight;
        const uint8_t cursorRow = selection.cursor / data.listColumns;
        const uint8_t first = cursorRow < rows ? 0 : (cursorRow - rows + 1) * data.listColumns;
        const uint8_t width = data.listRect.w / data.listColumns;
        for (uint8_t cell = 0; cell < rows * data.listColumns; ++cell)
        {
            const uint8_t index = first + cell;
            const uint16_t frame = index < selection.count ? frameFor(selection.entries[index]) : 0;
            const uint16_t key = frame | (index == selection.cursor ? selectedRow : 0);
            const Rect rect{static_cast<uint8_t>(data.listRect.x + (cell % data.listColumns) * width),
                            static_cast<uint8_t>(data.listRect.y + (cell / data.listColumns) * data.rowHeight), width, data.rowHeight};
            if (!full && key == cache.paintedRows[cell] && (!frame || !dirty.overlaps(rect))) continue;
            if (frame)
            {
                if (data.styledBrowse)
                {
                    const uint16_t styledFrame = confirmationFrame() + 1 + receiptCount + frame - firstEntryFrame;
                    if (!show(styledFrame + (index == selection.cursor), rect)) return false;
                }
                else
                {
                    if (!show(frame, rect)) return false;
                    if (index == selection.cursor) renderer.pixelDisplay().drawRect(rect.x, rect.y, rect.w, rect.h, 0x8758);
                }
            }
            else if (!full && !restoreBrowse(0, rect)) return false;
            cache.paintedRows[cell] = key;
            walletDirty |= rect.overlaps(data.walletRect);
        }
    }
    if (walletDirty) number(data.walletRect, balance);
    if (hasQuantity)
    {
        uint8_t slot = selected;
        if (!selection.inventory)
        {
            ShopData::File file;
            uint8_t row[ShopData::productSize];
            if (!data.open(file) || !data.product(file, selected, row)) return false;
            slot = row[6];
        }
        const uint16_t quantity = pet.persistentState().shopQuantities[slot];
        if (full || !cache.paintedHasQuantity || quantity != cache.paintedQuantity || dirty.overlaps(data.quantityRect) ||
            (walletDirty && data.walletRect.overlaps(data.quantityRect))) number(data.quantityRect, quantity);
        cache.paintedQuantity = quantity;
    }
    if (selection.view == View::Confirm)
    {
        if (!paintConfirmation(current, selected)) return false;
    }
    else if (!data.styledBrowse && selected >= ShopSelection::navigation && !data.rowHeight &&
             (pageChanged || (walletDirty && data.walletRect.overlaps(navigationCard))) &&
             !show(current + 1, navigationCard))
        return false;

    cache.paintedFrame = current;
    cache.paintedView = selection.view;
    cache.paintedWallet = balance;
    cache.paintedHasQuantity = hasQuantity;
    return paintFooter(full);
}

bool ShopView::Painter::paintConfirmation(uint16_t frame, uint8_t selected)
{
    ShopData::File file;
    uint8_t row[ShopData::itemSize];
    if (!data.open(file) || !(selection.inventory ? data.item(file, selected, row) : data.product(file, selected, row))) return false;
    if (!show(frame + 1, {0, 88, 128, 16}) ||
        !show(selection.inventory ? 15 : 14, {0, 104, 128, 16})) return false;
    number({0, 120, 60, 16}, selection.inventory ? pet.persistentState().shopQuantities[selected] : readU32(row));
    number({68, 120, 60, 16}, readU16(row + 4));
    return true;
}

bool ShopView::Painter::paintConfiguredConfirmation(uint8_t selected, bool buttonsOnly)
{
    const uint16_t background = confirmationFrame();
    ShopData::File file;
    if (!data.open(file)) return false;

    ShopConfirmationValues values{};
    ShopConfirmationNotice notice = ShopConfirmationNotice::None;
    if (selection.inventory)
        values.owned = pet.persistentState().shopQuantities[selected];
    else
    {
        uint8_t offer[ShopData::productSize], row[ShopData::itemSize];
        if (!data.product(file, selected, offer) || !data.item(file, offer[6], row)) return false;
        values = {readU32(offer), readU16(offer + 4), pet.persistentState().shopQuantities[offer[6]]};
        notice = shopPurchaseNotice(pet, config.stats[data.wallet].minValue, offer, row, values.owned, pet.customStat(data.wallet));
    }

    const uint16_t entry = selection.inventory ? selected : data.itemCount + selected;
    if (drawShopConfirmation(file, data.confirmationAddress(), entry, selection.cancel, values, notice, renderer,
                             {0, 0, data.art, 0, background}, buttonsOnly)) return true;
    data.recordError();
    return false;
}

bool ShopView::Painter::paintFooter(bool full)
{
    Footer footer = Footer::Browse;
    if (selection.view == View::Confirm)
        footer = selection.inventory ? (selection.cancel ? Footer::CancelUse : Footer::Use) : (selection.cancel ? Footer::CancelBuy : Footer::Buy);
    if (selection.view == View::Receipt) footer = selection.receipt;
    if (!full && footer == cache.paintedFooter) return true;
    cache.paintedFooter = footer;
    return show(footer, {0, 144, 128, 16});
}
#endif
