#include "PetStorage.h"

PetStorage::PetStorage(SdFat *ref_sd) : sd(ref_sd) {}

bool PetStorage::save(const Pet &pet)
{
    if (sd == nullptr)
        return false;

    File f = sd->open("/state.tmp", FILE_WRITE);
    if (!f)
        return false;

    const PersisState &state = pet.persistentState();
    f.seek(0);
    size_t n = f.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    f.flush();
    f.close();

    if (sd->exists("/state.bin"))
        sd->remove("/state.bin");
    if (!sd->rename("/state.tmp", "/state.bin"))
        return false;

    return n == sizeof(state);
}

bool PetStorage::load(Pet &pet)
{
    if (sd == nullptr)
        return false;

    File f = sd->open("/state.bin", FILE_READ);
    if (!f)
        return false;

    if (f.size() != sizeof(PersisState))
    {
        f.close();
        return false;
    }

    PersisState state = {};
    const size_t readCount = f.read(reinterpret_cast<uint8_t *>(&state), sizeof(state));
    f.close();

    if (readCount != sizeof(state))
        return false;

    return pet.restoreState(state);
}
