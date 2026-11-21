#include "game/CaptureAssets.h"
#include "common/AppProfile.h"

#if ENABLE_CAPTURE_GAME
#include "common/SdBinaryRead.h"

namespace
{
constexpr char path[] = "/capture.data";
constexpr uint8_t headerSize = 40;
constexpr uint8_t extraHeaderSize = 12;
constexpr uint8_t legacyTargetSize = 18;
constexpr uint8_t targetSize = 20;
constexpr uint8_t groupSize = 4;
constexpr uint8_t effectSize = 6;
constexpr uint8_t cardSize = 5;
constexpr uint8_t cardDirectoryFlag = 1;
constexpr uint8_t pauseReadingFlag = 2;
constexpr uint8_t timerPositionFlag = 4;
constexpr uint32_t backgroundBytes = 2UL * CaptureAssets::screenWidth * CaptureAssets::screenHeight;

uint16_t readU16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint32_t readU32(const uint8_t *bytes)
{
    return readU16(bytes) | (static_cast<uint32_t>(readU16(bytes + 2)) << 16);
}
}

CaptureAssets::~CaptureAssets() { close(); }

bool CaptureAssets::open(SdFat *sd)
{
    close();
    uint8_t header[headerSize];
    if (sd == nullptr || !file_.open(sd, path, FILE_READ) || !read(0, header, sizeof(header)))
    {
        close();
        return false;
    }

    // The exporter verifies the resource. Decode only the values the game uses.
    scene_.density = header[6] <= maxFish ? header[6] : maxFish;
    targetCount_ = header[7] <= maxTargets ? header[7] : maxTargets;
    scene_.direction = header[8];
    scene_.overlap = header[9];
    scene_.hookX = header[10];
    scene_.startY = header[11];
    scene_.depth = header[12];
    const bool extended = readU16(header + 4) == 2;
    const bool hasCards = extended && (header[15] & cardDirectoryFlag) != 0;
    scene_.pauseReading = (header[15] & pauseReadingFlag) != 0;
    scene_.timerX = 1;
    scene_.timerY = 0;
    if (extended && (header[15] & timerPositionFlag) != 0)
    {
        uint8_t position[2];
        if (!read(file_.fileSize() - sizeof(position), position, sizeof(position)))
        {
            close();
            return false;
        }
        scene_.timerX = position[0];
        scene_.timerY = position[1];
    }
    headerSize_ = headerSize;
    targetSize_ = legacyTargetSize;
    scene_.reward = {noSlot, 0, 1, 0, 0};
    uint8_t groupCount = 0;
    uint16_t effectCount = 0;
    if (extended)
    {
        uint8_t extra[extraHeaderSize];
        if (!read(headerSize, extra, sizeof(extra)))
        {
            close();
            return false;
        }
        headerSize_ += extraHeaderSize;
        targetSize_ = targetSize;
        groupCount = extra[0];
        effectCount = readU16(extra + 2);
        scene_.reward = {extra[1], readU16(extra + 4), readU16(extra + 6),
                         readU16(extra + 8), readU16(extra + 10)};
    }
    groupsOffset_ = headerSize_ + targetCount_ * targetSize_;
    effectsOffset_ = groupsOffset_ + groupCount * groupSize;
    backgroundOffset_ = effectsOffset_ + effectCount * effectSize;
    scene_.hook = {backgroundOffset_ + backgroundBytes, readU16(header + 22), header[13], header[14]};
    scene_.durationSeconds = readU16(header + 16);
    scene_.turnMs = readU16(header + 18);
    scene_.missPoints = static_cast<int16_t>(readU16(header + 20));

    totalWeight_ = 0;
    cardDirectory_ = 0;
    targetFingerprint_ = 0;
    uint8_t record[targetSize] = {};
    for (uint8_t slot = 0; slot < targetCount_; ++slot)
    {
        if (!read(headerSize_ + slot * targetSize_, record, targetSize_))
        {
            close();
            return false;
        }
        weights_[slot] = record[7] & 0x7f;
        totalWeight_ += weights_[slot];
    }
    if (hasCards)
    {
        // The card directory follows the last target's sprite pixels.
        const uint32_t trailer = readU32(record + 14) + 2UL * record[8] * record[9];
        uint8_t fingerprint[4];
        if (!read(trailer, fingerprint, sizeof(fingerprint)))
        {
            close();
            return false;
        }
        targetFingerprint_ = readU32(fingerprint);
        cardDirectory_ = trailer + sizeof(fingerprint);
    }
    return true;
}

void CaptureAssets::close()
{
    file_.close();
}

bool CaptureAssets::read(uint32_t offset, void *destination, size_t size)
{
    return file_.seekSet(offset) && readSdBinary(file_, destination, size) == static_cast<int>(size);
}

bool CaptureAssets::pickTarget(uint16_t roll, Target &target)
{
    uint8_t slot = 0;
    for (; slot < targetCount_; ++slot)
    {
        if (roll < weights_[slot]) break;
        roll -= weights_[slot];
    }
    uint8_t row[targetSize];
    if (slot == targetCount_ || !read(headerSize_ + slot * targetSize_, row, targetSize_)) return false;

    const bool extended = targetSize_ == targetSize;
    target = {
        {readU32(row + 14), readU16(row + 10), row[8], row[9]},
        static_cast<int16_t>(readU16(row + 12)), row[0], row[1], row[2], row[3], row[4],
        row[5], row[6], (row[7] & 0x80U) != 0,
        slot, extended ? row[18] : noSlot, extended ? row[19] : noSlot,
    };
    return true;
}

bool CaptureAssets::readGroup(uint8_t slot, Group &group)
{
    uint8_t row[groupSize];
    if (!read(groupsOffset_ + slot * groupSize, row, sizeof(row))) return false;
    group = {readU16(row), row[2]};
    return true;
}

bool CaptureAssets::readEffect(uint16_t index, Effect &effect)
{
    uint8_t row[effectSize];
    if (!read(effectsOffset_ + index * effectSize, row, sizeof(row))) return false;
    effect = {row[0], row[1], readU16(row + 2), static_cast<int16_t>(readU16(row + 4))};
    return true;
}

bool CaptureAssets::readBackground(uint16_t y, uint16_t rows, uint16_t *pixels)
{
    return read(backgroundOffset_ + 2UL * screenWidth * y, pixels, 2U * screenWidth * rows);
}

bool CaptureAssets::readSprite(const Sprite &sprite, uint16_t row, uint16_t rows, uint16_t *pixels)
{
    return read(sprite.offset + 2UL * sprite.width * row, pixels, 2U * sprite.width * rows);
}

bool CaptureAssets::readCard(uint8_t slot, Card &card)
{
    if (cardDirectory_ == 0)
    {
        card = {};
        return true;
    }
    uint8_t row[cardSize];
    if (!read(cardDirectory_ + slot * cardSize, row, sizeof(row))) return false;
    card = {readU32(row), row[4]};
    return true;
}

bool CaptureAssets::readCardRows(const Card &card, uint8_t page, uint16_t y, uint16_t rows, uint16_t *pixels)
{
    return read(card.offset + page * backgroundBytes + 2UL * screenWidth * y,
                pixels, 2U * screenWidth * rows);
}

#endif
