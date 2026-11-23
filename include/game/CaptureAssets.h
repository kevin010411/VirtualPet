#ifndef CAPTURE_ASSETS_H
#define CAPTURE_ASSETS_H

#include <stdint.h>
#include <SdFat.h>

class CaptureAssets
{
public:
    static constexpr int screenWidth = 128;
    static constexpr int screenHeight = 160;
    static constexpr int spriteMaxWidth = 64;
    static constexpr uint8_t maxFish = 6;
    static constexpr uint8_t noSlot = 255;
    static constexpr uint8_t stageDaysSlot = 12;

    struct Sprite
    {
        uint32_t offset;
        uint16_t transparent;
        uint8_t width;
        uint8_t height;
    };

    struct Reward
    {
        uint8_t stat;
        uint16_t participation, perPoints, coins, cap;
    };

    struct Group
    {
        uint16_t firstEffect;
        uint8_t effectCount;
    };

    struct Effect
    {
        uint8_t stat, mode;
        uint16_t limit;
        int16_t delta;
    };

    struct Scene
    {
        Sprite hook;
        Reward reward;
        uint16_t durationSeconds;
        uint16_t turnMs;
        int16_t missPoints;
        uint8_t density;
        uint8_t direction;
        uint8_t overlap;
        uint8_t hookX;
        uint8_t startY;
        uint8_t depth;
        uint8_t timerX, timerY;
        bool pauseReading;
    };

    struct Card
    {
        uint32_t offset;
        uint8_t pages;
    };

    struct Target
    {
        Sprite sprite;
        int16_t points;
        uint8_t x, y, width, height;
        uint8_t speed, hitWidth, hitHeight;
        bool facesLeft;
        uint8_t slot, group, collection;
    };

    ~CaptureAssets();
    bool open(SdFat *sd, uint8_t artworkSpeciesSlot = 0);
    void close();
    const Scene &scene() const { return scene_; }
    uint32_t targetFingerprint() const { return targetFingerprint_; }
    bool pickTarget(Target &target, uint8_t excludedSlot);
    bool readGroup(uint8_t slot, Group &group);
    bool readEffect(uint16_t index, Effect &effect);
    bool readBackground(uint16_t y, uint16_t rows, uint16_t *pixels);
    bool readSprite(const Sprite &sprite, uint16_t row, uint16_t rows, uint16_t *pixels);
    bool readCard(uint8_t slot, Card &card);
    bool readCardRows(const Card &card, uint8_t page, uint16_t y, uint16_t rows, uint16_t *pixels);

private:
    static constexpr uint8_t maxTargets = 64;
    bool read(uint32_t offset, void *destination, size_t size);
    SdBaseFile file_;
    Scene scene_ = {};
    uint32_t backgroundOffset_ = 0;
    uint32_t groupsOffset_ = 0;
    uint32_t effectsOffset_ = 0;
    uint32_t cardDirectory_ = 0;
    uint32_t targetFingerprint_ = 0;
    uint8_t headerSize_ = 0;
    uint8_t targetSize_ = 0;
    uint8_t weights_[maxTargets] = {};
    uint8_t targetCount_ = 0;
    uint16_t totalWeight_ = 0;
};

#endif
