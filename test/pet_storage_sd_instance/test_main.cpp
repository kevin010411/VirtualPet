#include "pet/adapters/PetStorage.h"

#include <assert.h>

int main()
{
    Pet pet;
    {
        SdFat sd;
        PetStorage storage(&sd);

        assert(storage.save(pet));
        assert(sd.exists("/state_a.bin"));

        Pet restored;
        assert(storage.load(restored, pet.persistentState().schemaFingerprint));
        assert(restored.speciesSlot() == pet.speciesSlot());
        assert(restored.outfitSlot() == pet.outfitSlot());
    }
    {
        SdFat sd;
        sd.failOpen(0x11, 0x22);
        PetStorage storage(&sd);
        assert(!storage.save(pet));
    }
    {
        SdFat sd;
        sd.seedReadOnly("/state_a.bin", {0x01, 0x02, 0x03});
        PetStorage storage(&sd);
        Pet ignored;
        assert(!storage.load(ignored, pet.persistentState().schemaFingerprint));
        assert(storage.save(pet));
    }
    {
        SdFat sd;
        sd.limitWrite(12, 0x33, 0x44);
        PetStorage storage(&sd);
        assert(!storage.save(pet));
    }
    {
        SdFat sd;
        sd.failSync(0x55, 0x66);
        PetStorage storage(&sd);
        assert(!storage.save(pet));
    }
    return 0;
}
