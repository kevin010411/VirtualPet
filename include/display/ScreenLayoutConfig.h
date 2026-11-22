#pragma once

#include "common/AppProfile.h"
#include "resources/AssetRuntimeContract.h"

constexpr uint8_t kMaxScreenBlocks = 32;
constexpr uint16_t kMaxScreenRules = APP_MAX_SCREEN_RULES;
constexpr uint8_t kMaxScreenRulesPerBlock = 50;
static_assert(kMaxScreenRules > 0 && kMaxScreenRules <= 1600,
              "Screen Rule capacity must fit 32 blocks of 50 segments.");
constexpr uint8_t kUnboundScreenSource = 255;

enum class ScreenBlockKind : uint8_t
{
    Animation = 1,
    Button = 2,
    Stat = 3,
    StatSum = 4,
    StatDimensions = 5,
    StatSumSegments = 6,
};

struct ScreenBlockConfig
{
    ScreenBlockKind kind;
    uint8_t source;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint16_t firstRule;
    uint8_t ruleCount;
    uint16_t fallbackFrame;
    uint32_t numericSettings = 0;

    bool isNumeric() const
    {
        return kind == ScreenBlockKind::Stat || kind == ScreenBlockKind::StatSum ||
               kind == ScreenBlockKind::StatDimensions || kind == ScreenBlockKind::StatSumSegments;
    }
};

struct ScreenRuleConfig
{
    uint32_t minimum;
    uint32_t maximum;
    uint16_t frame;
};
static_assert(sizeof(ScreenRuleConfig) == 12, "Screen rules store exported unsigned Stat ranges.");

struct ScreenLayoutConfig
{
    ScreenRuleConfig screenRules[kMaxScreenRules];
    ScreenBlockConfig screenBlocks[kMaxScreenBlocks];
    AssetData::AnimationRef layoutUnselected;
    AssetData::AnimationRef layoutSelected;
    uint16_t screenRuleCount;
    uint8_t screenBlockCount;
};
