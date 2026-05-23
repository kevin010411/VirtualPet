#include "PetStorage.h"

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

    const PersisState &state = pet.persistentState();
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
