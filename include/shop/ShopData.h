#pragma once

#include "display/DisplayRect.h"
#include "resources/AssetRuntimeContract.h"

class AssetResources;

class ShopData
{
public:
    static constexpr uint8_t itemSize = 129;
    static constexpr uint8_t productSize = 70;
    static constexpr uint8_t bandCount = 9;

    class File : public SdBaseFile
    {
    public:
        ~File() { close(); }
    };

    explicit ShopData(AssetResources &resources) : resources(resources) {}

    bool configure(bool enabled, const AssetData::BundleId &expectedBundle);
    bool open(File &file) const;
    bool item(SdBaseFile &file, uint8_t slot, uint8_t *row) const;
    bool product(SdBaseFile &file, uint8_t slot, uint8_t *row) const;
    bool arrival(SdBaseFile &file, uint8_t slot, uint8_t (&row)[9]) const;
    bool useAnimation(SdBaseFile &file, uint8_t slot, uint8_t species, uint16_t &animationId) const;
    bool pageBounds(uint16_t page, DisplayRect &bounds) const;
    bool browseBands(uint16_t page, uint8_t first, uint8_t count, uint8_t *frameIds) const;
    bool hasConfirmation() const { return confirmationOffset != 0; }
    bool hasBrowseBands() const { return bandsOffset != 0; }
    uint32_t confirmationAddress() const { return confirmationOffset; }
    SdFat *sdCard() const;
    void recordError() const;

    uint32_t fingerprint = 0;
    uint16_t art = 0;
    uint8_t wallet = 255, itemCount = 0, productCount = 0, rowHeight = 0;
    uint8_t listColumns = 1;
    bool styledBrowse = false;
    DisplayRect walletRect = {}, quantityRect = {}, listRect = {};

private:
    bool read(SdBaseFile &file, uint32_t offset, uint8_t *row, size_t length) const;
    AssetData::BundleId bundle = {};
    uint32_t boundsOffset = 0;
    uint32_t confirmationOffset = 0;
    uint32_t bandsOffset = 0;
    uint32_t itemOffset(uint8_t slot) const;

    AssetResources &resources;
    uint8_t speciesCount = 0;
};
