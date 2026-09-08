#include "pet/adapters/PetStorage.h"
#include "pet/adapters/PetStateSchemaDecision.h"
#include "shared/integrity/Crc32.h"
#include "shared/sd/SdBinaryRead.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
constexpr const char *kStateSlotAPath = "/state_a.bin";
constexpr const char *kStateSlotBPath = "/state_b.bin";
static_assert(sizeof(PersistedPetState) == 64, "Unexpected v12 pet state layout");

uint32_t calculateStateCrc(const PersistedPetState &state)
{
    return Integrity::crc32(
        reinterpret_cast<const uint8_t *>(&state),
        offsetof(PersistedPetState, crc32));
}

bool isSequenceNewer(uint32_t candidate, uint32_t current)
{
    return static_cast<int32_t>(candidate - current) > 0;
}

bool readStateSlot(SdFat *sd, const char *path, PersistedPetState &state)
{
    if (sd == nullptr)
        return false;
    SdBaseFile f;
    if (!f.open(sd, path, FILE_READ))
        return false;

    const size_t stateSize = f.fileSize();
    if (stateSize != sizeof(PersistedPetState))
    {
        f.close();
        return false;
    }

    state = {};
    const int readCount = readSdBinary(f, &state, stateSize);
    f.close();

    if (readCount != static_cast<int>(stateSize))
        return false;

    if (state.magic != Pet::kPetStateMagic)
        return false;

    if (calculateStateCrc(state) != state.crc32)
        return false;

    return state.version == Pet::kPetStateVersion;
}

#if ENABLE_DEBUG
bool removeStateSlot(SdFat *sd, const char *path)
{
    if (sd == nullptr || !sd->exists(path))
        return true;
    if (sd->remove(path))
        return true;

    // A corrupt save copied from another host can retain the FAT read-only
    // attribute. It is app-owned state, so clear that attribute before the
    // retry instead of permanently preventing a fresh pet from starting.
    SdBaseFile stale;
    if (!stale.open(sd, path, O_RDONLY))
        return false;
    const int attributes = stale.attrib();
    const bool madeWritable = attributes >= 0 &&
                              stale.attrib(static_cast<uint8_t>(
                                  attributes & ~FS_ATTRIB_READ_ONLY));
    stale.close();
    return madeWritable && sd->remove(path);
}
#endif

void discardSave(SdFat *sd)
{
#if ENABLE_DEBUG
    removeStateSlot(sd, kStateSlotAPath);
    removeStateSlot(sd, kStateSlotBPath);
#else
    sd->remove(kStateSlotAPath);
    sd->remove(kStateSlotBPath);
#endif
}

bool writeStateSlot(SdFat *sd, const char *path, const PersistedPetState &state)
{
    if (sd == nullptr)
        return false;
    SdBaseFile f;
    if (!f.open(sd, path, O_WRONLY | O_CREAT | O_TRUNC))
    {
#if ENABLE_DEBUG
        if (!sd->exists(path) || !removeStateSlot(sd, path) ||
            !f.open(sd, path, O_WRONLY | O_CREAT | O_TRUNC))
            return false;
#else
        return false;
#endif
    }

    const size_t written = f.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    if (written != sizeof(state))
    {
        f.close();
        return false;
    }
    const bool synced = f.sync();
    f.close();
    return synced;
}
} // namespace

PetStorage::PetStorage(SdFat *ref_sd) : sd(ref_sd) {}

bool PetStorage::save(const Pet &pet)
{
    attemptedSaveSlot = writeSlotA ? 'A' : 'B';
    attemptedSaveSequence = nextSequence;

    if (sd == nullptr)
        return false;

    PersistedPetState state = pet.persistentState();
    state.sequence = nextSequence;
    state.crc32 = calculateStateCrc(state);

    const char *path = writeSlotA ? kStateSlotAPath : kStateSlotBPath;
    if (!writeStateSlot(sd, path, state))
        return false;

    ++nextSequence;
    writeSlotA = !writeSlotA;
    return true;
}

char PetStorage::lastSaveSlot() const
{
    return attemptedSaveSlot;
}

uint32_t PetStorage::lastSaveSequence() const
{
    return attemptedSaveSequence;
}

bool PetStorage::load(Pet &pet, uint32_t schemaFingerprint)
{
    if (sd == nullptr)
        return false;

    PersistedPetState stateA = {};
    PersistedPetState stateB = {};
    const bool hasA = readStateSlot(sd, kStateSlotAPath, stateA);
    const bool hasB = readStateSlot(sd, kStateSlotBPath, stateB);

    if (!hasA && !hasB)
    {
        if (sd->exists(kStateSlotAPath) || sd->exists(kStateSlotBPath))
            discardSave(sd);
        return false;
    }

    const PersistedPetState &selected = (!hasB || (hasA && isSequenceNewer(stateA.sequence, stateB.sequence)))
                                           ? stateA
                                           : stateB;
    if (decidePetStateSchema(selected.schemaFingerprint, schemaFingerprint) ==
        PetStateSchemaDecision::Reset)
    {
        discardSave(sd);
        nextSequence = 1;
        writeSlotA = true;
        return false;
    }
    const bool selectedSlotA = (&selected == &stateA);
    if (!pet.restoreState(selected))
        return false;

    nextSequence = selected.sequence + 1;
    writeSlotA = !selectedSlotA;
    return true;
}

void PetStorage::discard()
{
    if (sd != nullptr)
        discardSave(sd);
    nextSequence = 1;
    writeSlotA = true;
}
