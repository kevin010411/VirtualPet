#include "storage/PetStorage.h"

#include <stddef.h>

namespace
{
constexpr const char *kStatePath = "/state.bin";
constexpr const char *kBackupStatePath = "/state.bak";
constexpr const char *kTempStatePath = "/state.tmp";

bool loadStateFile(SdFat *sd, const char *path, Pet &pet)
{
    File f = sd->open(path, FILE_READ);
    if (!f)
        return false;

    const size_t stateSize = f.size();
    const size_t legacyStateSize = offsetof(PersistedPetState, species);
    const size_t v2StateSize = offsetof(PersistedPetState, healthy_days);
    if (stateSize == v2StateSize)
    {
        f.close();
        sd->remove(path);
        return false;
    }

    if (stateSize != sizeof(PersistedPetState) &&
        stateSize != legacyStateSize)
    {
        f.close();
        return false;
    }

    PersistedPetState state = {};
    const size_t readCount = f.read(reinterpret_cast<uint8_t *>(&state), stateSize);
    f.close();

    if (readCount != stateSize)
        return false;

    return pet.restoreState(state);
}
} // namespace

PetStorage::PetStorage(SdFat *ref_sd) : sd(ref_sd) {}

bool PetStorage::save(const Pet &pet)
{
    if (sd == nullptr)
        return false;

    if (sd->exists(kTempStatePath) && !sd->remove(kTempStatePath))
        return false;

    File f = sd->open(kTempStatePath, FILE_WRITE);
    if (!f)
        return false;

    const PersistedPetState &state = pet.persistentState();
    const size_t n = f.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    f.flush();
    f.close();

    if (n != sizeof(state))
    {
        sd->remove(kTempStatePath);
        return false;
    }

    if (sd->exists(kStatePath))
    {
        if (sd->exists(kBackupStatePath) && !sd->remove(kBackupStatePath))
            return false;
        if (!sd->rename(kStatePath, kBackupStatePath))
            return false;
    }

    return sd->rename(kTempStatePath, kStatePath);
}

bool PetStorage::load(Pet &pet)
{
    if (sd == nullptr)
        return false;

    return loadStateFile(sd, kStatePath, pet) ||
           loadStateFile(sd, kBackupStatePath, pet);
}
