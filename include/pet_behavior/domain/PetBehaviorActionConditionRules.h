#ifndef PET_BEHAVIOR_ACTION_CONDITION_RULES_H
#define PET_BEHAVIOR_ACTION_CONDITION_RULES_H

#include <stdint.h>

#include "pet_behavior/domain/PetBehaviorTypes.h"

struct PetBehaviorActionConditionInterval
{
    int64_t minimum;
    int64_t maximum;
};

bool parsePetBehaviorActionConditionOperator(
    const char *token,
    PetBehaviorActionConditionOperator &comparison);
bool petBehaviorActionConditionInterval(
    PetBehaviorActionConditionOperator comparison,
    int32_t threshold,
    int64_t domainMinimum,
    int64_t domainMaximum,
    PetBehaviorActionConditionInterval &interval);
bool petBehaviorActionConditionMatches(
    PetBehaviorActionConditionOperator comparison,
    int32_t threshold,
    int64_t current);

#endif // PET_BEHAVIOR_ACTION_CONDITION_RULES_H
