#include "storage/PetStorage.h"

#include <stddef.h>
#include <stdint.h>

namespace
{
constexpr const char *kStateSlotAPath = "/state_a.bin";
constexpr const char *kStateSlotBPath = "/state_b.bin";

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

    if (state.magic != 0x50455431UL || state.version != 5)
        return false;

    return calculateStateCrc(state) == state.crc32;
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

bool PetStorage::load(Pet &pet)
{
    if (sd == nullptr)
        return false;

    PersistedPetState stateA = {};
    PersistedPetState stateB = {};
    const bool hasA = readStateSlot(sd, kStateSlotAPath, stateA);
    const bool hasB = readStateSlot(sd, kStateSlotBPath, stateB);

    if (!hasA && !hasB)
        return false;

    const PersistedPetState &selected = (!hasB || (hasA && isSequenceNewer(stateA.sequence, stateB.sequence)))
                                           ? stateA
                                           : stateB;
    const bool selectedSlotA = (&selected == &stateA);
    if (!pet.restoreState(selected))
        return false;

    nextSequence = selected.sequence + 1;
    writeSlotA = !selectedSlotA;
    return true;
}
