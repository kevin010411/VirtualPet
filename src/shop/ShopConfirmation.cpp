#include "shop/ShopConfirmation.h"
#if ENABLE_SHOP

#include <algorithm>
#include "common/SdBinaryRead.h"
#include "display/DisplayRect.h"
#include "display/Renderer.h"
#include "resources/detail/RuntimeTableReader.h"

using RuntimeTableInternal::readI16;
using RuntimeTableInternal::readU16;
using RuntimeTableInternal::readU32;

namespace
{
constexpr uint8_t blockHeader = 20;
constexpr uint8_t referenceSize = 5;
constexpr uint8_t templateGlyphs = 11;
constexpr uint8_t commonBlocks = 6;
constexpr uint8_t maxPageBlocks = 5;
constexpr DisplayRect screen{0, 0, 128, 160};
constexpr uint32_t endOfLine = UINT32_MAX;

DisplayRect clipRect(int x, int y, int width, int height, const DisplayRect &region)
{
    const int left = std::max(x, static_cast<int>(region.x));
    const int top = std::max(y, static_cast<int>(region.y));
    const int right = std::min(x + width, region.right());
    const int bottom = std::min(y + height, region.bottom());
    if (left >= right || top >= bottom) return {};
    return {static_cast<uint8_t>(left), static_cast<uint8_t>(top),
            static_cast<uint8_t>(right - left), static_cast<uint8_t>(bottom - top)};
}

struct GlyphRef
{
    uint32_t offset;
    uint8_t advance;
};

GlyphRef glyphRef(const uint8_t *row)
{
    return {readU32(row), row[4]};
}

class ConfirmationFile
{
public:
    ConfirmationFile(SdBaseFile &file, uint32_t start) : file(file), start(start) {}

    bool read(uint32_t offset, uint8_t *row, size_t count)
    {
        return file.seekSet(start + offset) && readSdBinary(file, row, count) == static_cast<int>(count);
    }

    bool index(uint16_t entry, uint32_t &offset)
    {
        uint8_t row[4];
        if (!read(4 + 4U * entry, row, sizeof(row))) return false;
        offset = readU32(row);
        return true;
    }

private:
    SdBaseFile &file;
    uint32_t start;
};

struct Digits
{
    explicit Digits(uint32_t value)
    {
        do
        {
            reversed[count++] = value % 10;
            value /= 10;
        } while (value);
    }

    uint8_t at(uint8_t index) const { return reversed[count - 1 - index]; }

    uint8_t reversed[10];
    uint8_t count = 0;
};

class GlyphCursor
{
public:
    GlyphCursor(ConfirmationFile &file, uint32_t offset, uint16_t count, const uint8_t *table, const Digits *values)
        : file(file), offset(offset), count(count), table(table), values(values) {}

    bool next(GlyphRef &glyph)
    {
        if (!decimal || digit == decimal->count)
        {
            if (position == count)
            {
                glyph = {endOfLine, 0};
                return true;
            }
            uint8_t row[referenceSize];
            if (!file.read(offset + referenceSize * position++, row, sizeof(row))) return false;
            glyph = glyphRef(row);
            if (glyph.offset >= 3) return true;
            decimal = &values[glyph.offset];
            digit = 0;
        }
        glyph = glyphRef(table + referenceSize * decimal->at(digit++));
        return true;
    }

private:
    ConfirmationFile &file;
    uint32_t offset;
    uint16_t count, position = 0;
    const uint8_t *table;
    const Digits *values;
    const Digits *decimal = nullptr;
    uint8_t digit = 0;
};

bool stampGlyph(ConfirmationFile &file, const ShopConfirmationBlock &block,
                ShopConfirmationBlock::Mask &mask, int pen, GlyphRef glyph)
{
    uint8_t header[4];
    if (!file.read(glyph.offset, header, sizeof(header))) return false;
    const int left = pen + static_cast<int8_t>(header[0]);
    const uint8_t top = header[1], width = header[2], height = header[3];
    if (!width) return true;
    const uint8_t stride = (width + 7) / 8;
    uint8_t bitmap[8 * ShopConfirmationBlock::maxHeight];
    if (!file.read(glyph.offset + sizeof(header), bitmap, stride * height)) return false;
    const uint8_t inset = block.action ? 1 : 0;
    for (uint8_t y = 0; y < height; ++y)
        for (uint8_t column = 0; column < width; ++column)
        {
            const int x = left + column;
            if (x >= inset && x < block.width - inset && (bitmap[y * stride + column / 8] & (0x80 >> (column % 8))))
                mask[top + y][x / 8] |= 0x80 >> (x % 8);
        }
    return true;
}

void paintBlock(Renderer &renderer, const ShopConfirmationBlock &block, const uint8_t *mask,
                bool selected, const DisplayRect &visible);

bool drawBlock(ConfirmationFile &file, uint32_t offset, const Digits *values, bool returning,
               Renderer &renderer, const DisplayRect &region)
{
    uint8_t row[blockHeader + referenceSize * templateGlyphs];
    if (!file.read(offset, row, sizeof(row))) return false;
    const ShopConfirmationBlock block{
        readI16(row), readI16(row + 2), row[4], row[5], row[6], row[7] != 0,
        readU16(row + 8), readU16(row + 10), readU16(row + 12), readU16(row + 14), readU16(row + 16),
    };
    const DisplayRect visible = clipRect(block.x, block.y, block.width, block.height, region);
    if (!visible.w) return true;

    const uint8_t *table = row + blockHeader;
    const GlyphRef ellipsis = glyphRef(table + 10 * referenceSize);
    const GlyphCursor start(file, offset + sizeof(row), readU16(row + 18), table, values);
    GlyphCursor measure = start;
    const int padding = (block.action ? 1 : 0) + (block.outlined ? 1 : 0);
    const int available = block.width - 2 * padding;
    int width = 0, prefixWidth = 0;
    unsigned count = 0, prefixCount = 0;
    GlyphRef glyph;
    while (true)
    {
        if (!measure.next(glyph)) return false;
        if (glyph.offset == endOfLine) break;
        width += glyph.advance;
        ++count;
        if (width <= available - ellipsis.advance)
        {
            prefixWidth = width;
            prefixCount = count;
        }
        if (width > available) break;
    }

    const bool overflow = width > available;
    const bool addEllipsis = overflow && ellipsis.advance <= available;
    if (overflow)
    {
        count = prefixCount;
        width = prefixWidth + (addEllipsis ? ellipsis.advance : 0);
    }
    int pen = padding + (available - width) / 2;
    ShopConfirmationBlock::Mask mask{};
    GlyphCursor paint = start;
    for (unsigned index = 0; index < count; ++index)
    {
        if (!paint.next(glyph) || !stampGlyph(file, block, mask, pen, glyph)) return false;
        pen += glyph.advance;
    }
    if (addEllipsis && !stampGlyph(file, block, mask, pen, ellipsis)) return false;
    paintBlock(renderer, block, reinterpret_cast<const uint8_t *>(&mask),
               block.action == (returning ? 2 : 1), visible);
    return true;
}

bool inkAt(const ShopConfirmationBlock &block, const uint8_t *mask, int x, int y)
{
    return x >= 0 && x < block.width && y >= 0 && y < block.height &&
           (mask[y * ShopConfirmationBlock::maskStride + x / 8] & (0x80 >> (x % 8)));
}

bool outlineAt(const ShopConfirmationBlock &block, const uint8_t *mask, int x, int y)
{
    for (int row = y - 1; row <= y + 1; ++row)
        for (int column = x - 1; column <= x + 1; ++column)
            if (inkAt(block, mask, column, row)) return true;
    return false;
}
}

bool drawShopConfirmation(SdBaseFile &file, uint32_t offset, uint16_t entry, bool returning,
                          ShopConfirmationValues values, ShopConfirmationNotice notice, Renderer &renderer,
                          const AssetData::AssetFrameAddress &background, bool buttonsOnly)
{
    ConfirmationFile source(file, offset);
    const Digits digits[]{Digits(values.price), Digits(values.quantity), Digits(values.owned)};
    uint32_t blocks[2 + maxPageBlocks];
    uint8_t blockCount = 1;
    if (!source.index(0, blocks[0])) return false;
    if (notice != ShopConfirmationNotice::None &&
        !source.index(static_cast<uint8_t>(notice), blocks[blockCount++]))
        return false;

    uint32_t page;
    uint8_t count, offsets[maxPageBlocks * 4];
    if (!source.index(commonBlocks + entry, page) || !source.read(page, &count, 1) ||
        !source.read(page + 1, offsets, count * 4U)) return false;
    for (uint8_t index = 0; index < count; ++index)
        blocks[blockCount++] = readU32(offsets + 4 * index);

    const auto paint = [&](const DisplayRect &region) {
        if (!renderer.ShowDataFrame(background, 0, 0, 12, screen.w, screen.h, buttonsOnly ? &region : nullptr))
            return false;
        for (uint8_t index = 0; index < blockCount; ++index)
            if (!drawBlock(source, blocks[index], digits, returning, renderer, region)) return false;
        return true;
    };
    if (!buttonsOnly) return paint(screen);

    // Restore each button's region in layer order so moved buttons can overlap other text.
    for (uint8_t index = blockCount - 2; index < blockCount; ++index)
    {
        uint8_t bounds[6];
        if (!source.read(blocks[index], bounds, sizeof(bounds))) return false;
        const auto region = clipRect(readI16(bounds), readI16(bounds + 2), bounds[4], bounds[5], screen);
        if (region.w && !paint(region)) return false;
    }
    return true;
}

namespace
{
void paintBlock(Renderer &renderer, const ShopConfirmationBlock &block, const uint8_t *mask,
                bool selected, const DisplayRect &visible)
{
    auto &display = renderer.pixelDisplay();
    auto border = [&display, &visible, &block](int x, int y, int width, int height) {
        const auto rect = clipRect(x, y, width, height, visible);
        if (rect.w) display.writeFillRect(rect.x, rect.y, rect.w, rect.h, block.border);
    };
    display.startWrite();
    if (selected)
        display.writeFillRect(visible.x, visible.y, visible.w, visible.h, block.fill);
    else if (block.action)
    {
        border(block.x, block.y, block.width, 1);
        border(block.x, block.y + block.height - 1, block.width, 1);
        border(block.x, block.y + 1, 1, block.height - 2);
        border(block.x + block.width - 1, block.y + 1, 1, block.height - 2);
    }

    const uint16_t ink = selected ? block.selectedInk : block.ink;
    const int inset = block.action ? 1 : 0;
    const int left = std::max(inset, visible.x - block.x);
    const int right = std::min(block.width - inset, visible.right() - block.x);
    const int top = std::max(inset, visible.y - block.y);
    const int bottom = std::min(block.height - inset, visible.bottom() - block.y);
    for (int y = top; y < bottom; ++y)
    {
        int run = left, previous = -1;
        for (int x = left; x <= right; ++x)
        {
            int color = -1;
            if (x < right)
            {
                if (inkAt(block, mask, x, y)) color = ink;
                else if (block.outlined && outlineAt(block, mask, x, y)) color = block.outline;
            }
            if (color == previous) continue;
            if (previous >= 0) display.writeFillRect(block.x + run, block.y + y, x - run, 1, previous);
            run = x;
            previous = color;
        }
    }
    display.endWrite();
}
}
#endif
