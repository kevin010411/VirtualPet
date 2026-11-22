#include "common/AppProfile.h"
#if ENABLE_SHOP
#include "shop/ShopData.h"

#include <string.h>
#include "common/SdBinaryRead.h"
#include "resources/AssetResources.h"
#include "resources/detail/RuntimeTableReader.h"

using RuntimeTableInternal::readU16;
using RuntimeTableInternal::readU32;

namespace
{
constexpr char path[] = "/shop.data";
constexpr uint8_t headerSize = 39;
}

void ShopData::recordError() const
{
    resources.recordAssetDataErrorResource(path);
}

bool ShopData::open(File &file) const
{
    if (file.open(path, FILE_READ)) return true;
    recordError();
    return false;
}

bool ShopData::read(SdBaseFile &file, uint32_t offset, uint8_t *row, size_t length) const
{
    if (file.seekSet(offset) && readSdBinary(file, row, length) == static_cast<int>(length))
        return true;
    recordError();
    return false;
}

bool ShopData::configure(bool enabled, const AssetData::BundleId &expectedBundle)
{
    if (!enabled)
    {
        art = 0;
        return true;
    }
    if (art && AssetData::sameBundleId(bundle, expectedBundle))
        return true;
    art = 0;
    File file;
    uint8_t row[headerSize];
    if (!open(file) || !read(file, 0, row, sizeof(row))) return false;
    memcpy(bundle.bytes, row, sizeof(bundle.bytes));
    if (!AssetData::sameBundleId(bundle, expectedBundle))
    {
        recordError();
        return false;
    }
    fingerprint = readU32(row + 16);
    const uint16_t newArt = readU16(row + 20);
    wallet = row[22];
    itemCount = row[23];
    productCount = row[24];
    rowHeight = row[25];
    speciesCount = row[26];
    memcpy(&walletRect, row + 27, sizeof(DisplayRect));
    memcpy(&quantityRect, row + 31, sizeof(DisplayRect));
    memcpy(&listRect, row + 35, sizeof(DisplayRect));
    // The optional trailer leaves the original Shop records at their old offsets.
    const uint32_t recordsEnd = itemOffset(itemCount) + static_cast<uint32_t>(productSize) * productCount;
    boundsOffset = !rowHeight && file.fileSize() > recordsEnd ? recordsEnd : 0;
    const uint32_t boundsEnd = recordsEnd + (rowHeight ? 0 : sizeof(DisplayRect) * (itemCount + productCount + 3));
    confirmationOffset = 0;
    bandsOffset = 0;
    styledBrowse = false;
    listColumns = 1;
    uint8_t signature[4];
    if (file.fileSize() >= boundsEnd + sizeof(signature))
    {
        if (!read(file, boundsEnd, signature, sizeof(signature))) return false;
        if (memcmp(signature, "SCF1", sizeof(signature)) == 0) confirmationOffset = boundsEnd;
    }
    const uint16_t bandBytes = 2 * bandCount * (rowHeight ? 1 : itemCount + productCount + 4);
    if (confirmationOffset && file.fileSize() >= boundsEnd + bandBytes + sizeof(signature))
    {
        const uint32_t tagOffset = file.fileSize() - sizeof(signature);
        if (!read(file, tagOffset, signature, sizeof(signature))) return false;
        styledBrowse = memcmp(signature, "SBD2", sizeof(signature)) == 0;
        if (styledBrowse || memcmp(signature, "SBD1", sizeof(signature)) == 0)
            bandsOffset = tagOffset - bandBytes;
        if (styledBrowse && !read(file, bandsOffset - 1, &listColumns, sizeof(listColumns))) return false;
    }
    art = newArt;
    return true;
}

uint32_t ShopData::itemOffset(uint8_t slot) const
{
    return headerSize + static_cast<uint32_t>(itemSize + 2 * speciesCount) * slot;
}

bool ShopData::item(SdBaseFile &file, uint8_t slot, uint8_t *row) const
{
    return read(file, itemOffset(slot), row, itemSize);
}

bool ShopData::product(SdBaseFile &file, uint8_t slot, uint8_t *row) const
{
    return read(file, itemOffset(itemCount) + static_cast<uint32_t>(productSize) * slot, row, productSize);
}

bool ShopData::arrival(SdBaseFile &file, uint8_t slot, uint8_t (&row)[9]) const
{
    return read(file, itemOffset(slot), row, sizeof(row));
}

bool ShopData::useAnimation(SdBaseFile &file, uint8_t slot, uint8_t species, uint16_t &animationId) const
{
    uint8_t binding[2];
    if (!read(file, itemOffset(slot) + itemSize + 2 * (species - 1U), binding, sizeof(binding))) return false;
    animationId = readU16(binding);
    return true;
}
bool ShopData::pageBounds(uint16_t page, DisplayRect &bounds) const
{
    if (!boundsOffset) return true;
    File file;
    return open(file) &&
           read(file, boundsOffset + sizeof(bounds) * page,
                reinterpret_cast<uint8_t *>(&bounds), sizeof(bounds));
}

bool ShopData::browseBands(uint16_t page, uint8_t first, uint8_t count, uint8_t *frameIds) const
{
    File file;
    return open(file) && read(file, bandsOffset + 2 * (page * bandCount + first), frameIds, 2 * count);
}

SdFat *ShopData::sdCard() const
{
    return resources.sdCard();
}
#endif
