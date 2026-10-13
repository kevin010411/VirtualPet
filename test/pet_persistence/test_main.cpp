#include <assert.h>
#include <string.h>

#include "pet/Pet.h"

int main()
{
    Pet pet;
    PersistedPetState state = pet.persistentState();

    strcpy(state.speciesSlotText, "dino");
    strcpy(state.outfitSlotText, "base");

    assert(!pet.restoreState(state));
    assert(pet.speciesSlot() == 1);
    assert(pet.outfitSlot() == 1);

    state = pet.persistentState();
    state.customStats[0] = 10;
    assert(pet.restoreState(state));
    pet.configureDeath(true, 1);
    assert(pet.setCustomStat(0, 0));
    assert(pet.isDead() == bool(ENABLE_DEATH));

    // A saved Dead remains locked even when new death is compiled out.
    state.flowFlags |= 0x4UL;
    assert(pet.restoreState(state));
    pet.configureDeath(false, 0);
    assert(pet.evaluateDeath());
    assert(!pet.setCustomStat(0, 10));
    assert(!pet.commitPetDay(state.customStats, kPetCustomStatCount));

    // New-life reset clears Dead; disabled builds cannot die again.
    pet.setDefaultState();
    assert(!pet.isDead());
    pet.configureDeath(bool(ENABLE_DEATH), 1);
    assert(pet.setCustomStat(0, 10));
    assert(pet.setCustomStat(0, 0));
    assert(pet.isDead() == bool(ENABLE_DEATH));
    return 0;
}
