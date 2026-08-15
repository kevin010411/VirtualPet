#include "pet/adapters/PetStorage.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
constexpr const char *kStateSlotAPath = "/state_a.bin";
constexpr const char *kStateSlotBPath = "/state_b.bin";
static_assert(sizeof(PersistedPetState) == 64, "Unexpected v12 pet state layout");

uint32_t crc32Bitwise(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            const uint32_t mask = (crc & 1UL) ? 0xEDB88320UL : 0;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

uint32_t calculateStateCrc(const PersistedPetState &state)
{
    return crc32Bitwise(reinterpret_cast<const uint8_t *>(&state), offsetof(PersistedPetState, crc32));
}

bool isSequenceNewer(uint32_t candidate, uint32_t current)
{
    return static_cast<int32_t>(candidate - current) > 0;
}

bool readStateSlot(SdFat *sd, const char *path, PersistedPetState &state)
{
    File f = sd->open(path, FILE_READ);
    if (!f)
        return false;

    const size_t stateSize = f.size();
    if (stateSize != sizeof(PersistedPetState))
    {
        f.close();
        return false;
    }

    state = {};
    const size_t readCount = f.read(reinterpret_cast<uint8_t *>(&state), stateSize);
    f.close();

    if (readCount != stateSize)
        return false;

    if (state.magic != Pet::kPetStateMagic)
        return false;

    if (calculateStateCrc(state) != state.crc32)
        return false;

    return state.version == Pet::kPetStateVersion;
}

void discardSave(SdFat *sd)
{
    sd->remove(kStateSlotAPath);
    sd->remove(kStateSlotBPath);
}

bool writeStateSlot(SdFat *sd, const char *path, const PersistedPetState &state)
{
    File f = sd->open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!f)
        return false;

    const size_t n = f.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    f.flush();
    f.close();

    return n == sizeof(state);
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
    if (selected.schemaFingerprint != schemaFingerprint)
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
