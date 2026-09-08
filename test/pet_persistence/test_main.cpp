#include <assert.h>
#include <string.h>

#include "pet/domain/Pet.h"

int main()
{
    Pet pet;
    PersistedPetState state = pet.persistentState();

    strcpy(state.speciesSlotText, "dino");
    strcpy(state.outfitSlotText, "base");

    assert(!pet.restoreState(state));
    assert(pet.speciesSlot() == 1);
    assert(pet.outfitSlot() == 1);
    return 0;
}
