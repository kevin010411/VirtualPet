#ifndef CUSTOM_RULES_H
#define CUSTOM_RULES_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/domain/Animation.h"

class AnimationController;
class DebugDisplay;
class Pet;
class PetActionController;

class CustomRules
{
public:
    static constexpr uint8_t kStatCount = 8;
    static constexpr uint8_t kMaxDailyRules = 8;
    static constexpr uint8_t kMaxActionRules = 16;

    struct StatRule
    {
        bool defined = false;
        char label[16] = {};
        int16_t initialValue = 0;
        int16_t minValue = 0;
        int16_t maxValue = 0;
    };

    struct DailyRule
    {
        uint8_t slot = 0;
        int16_t delta = 0;
    };

    struct ActionRule
    {
        char key[16] = {};
        uint8_t targetSlot = 0;
        uint8_t requiredSlot = 0;
        int16_t requiredMin = 0;
        int16_t delta = 0;
        char animation[16] = {};
    };

    bool load(SdFat *sd, DebugDisplay *debug = nullptr);
    void clear();
    bool isEnabled() const;
    bool hasAction(const char *key) const;

    void applyInitialValues(Pet &pet) const;
    void clampValues(Pet &pet) const;
    void applyDaily(PetActionController &petActions) const;
    bool executeAction(const char *key, PetActionController &petActions, AnimationController &animations) const;

private:
    StatRule stats[kStatCount] = {};
    DailyRule dailyRules[kMaxDailyRules] = {};
    ActionRule actionRules[kMaxActionRules] = {};
    uint8_t dailyRuleCount = 0;
    uint8_t actionRuleCount = 0;
    bool enabled = false;

    const ActionRule *findAction(const char *key) const;
};

#endif // CUSTOM_RULES_H
