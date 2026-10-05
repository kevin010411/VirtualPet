#ifndef EVOLUTION_CONDITION_CONTRACT_H
#define EVOLUTION_CONDITION_CONTRACT_H

#include "pet/Pet.h"
#include "pet/PetBehaviorStatSlot.h"

bool evaluateEvolutionConditions(
    char *conditions,
    const PetStatSnapshot &stats,
    const ActivePetBehaviorStatSlots &activeSlots);

#endif // EVOLUTION_CONDITION_CONTRACT_H
