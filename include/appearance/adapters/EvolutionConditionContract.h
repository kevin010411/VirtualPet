#ifndef EVOLUTION_CONDITION_CONTRACT_H
#define EVOLUTION_CONDITION_CONTRACT_H

#include "pet/domain/Pet.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"

bool evaluateEvolutionConditions(
    char *conditions,
    const PetStatSnapshot &stats,
    const ActivePetBehaviorStatSlots &activeSlots);

#endif // EVOLUTION_CONDITION_CONTRACT_H
