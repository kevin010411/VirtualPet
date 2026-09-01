#include "pet/domain/Pet.h"

#include <string.h>
#include "shared/utils/CanonicalDecimal.h"

namespace
{
constexpr uint32_t kFirstLaunchCompleteFlag = 0x1UL;
constexpr uint32_t kFirstStartCompletedFlag = 0x2UL;

bool copyPersistedAppearanceText(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr || source[0] == '\0')
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = source[i];
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    strcpy(dest, source);
    return true;
}

uint8_t parseAppearanceSlot(const char *text)
{
    uint32_t value = 0;
    if (!CanonicalDecimal::parseUnsigned(text, UINT8_MAX, value, false))
        return 0;
    return static_cast<uint8_t>(value);
}

bool writeAppearanceSlot(char *destination, size_t destinationSize, uint8_t slot)
{
    if (destination == nullptr || slot == 0)
        return false;
    char reversed[3] = {};
    size_t digitCount = 0;
    do
    {
        reversed[digitCount++] = static_cast<char>('0' + (slot % 10U));
        slot = static_cast<uint8_t>(slot / 10U);
    } while (slot != 0);
    if (digitCount >= destinationSize)
        return false;
    for (size_t index = 0; index < digitCount; ++index)
        destination[index] = reversed[digitCount - index - 1U];
    destination[digitCount] = '\0';
    return true;
}
} // namespace

Pet::Pet()
{
    setDefaultState();
}

bool Pet::commitPetDay(const int16_t *customStats, size_t customStatCount)
{
    if (customStats == nullptr || customStatCount == 0 || customStatCount > kPetCustomStatCount)
        return false;

    const uint32_t nextStageDays = st.stage_days == UINT32_MAX ? UINT32_MAX : st.stage_days + 1;
    st.stage_days = nextStageDays;
    return commitPetStats(customStats, customStatCount);
}

bool Pet::commitPetStats(const int16_t *customStats, size_t customStatCount)
{
    if (customStats == nullptr || customStatCount == 0 || customStatCount > kPetCustomStatCount)
        return false;

    for (size_t index = 0; index < customStatCount; ++index)
        st.customStats[index] = customStats[index];
    return true;
}

void Pet::setDefaultState()
{
    st = {};
    st.magic = kPetStateMagic;
    st.version = kPetStateVersion;
    st.sequence = 0;
    st.schemaFingerprint = 0;
    strcpy(st.speciesSlotText, "1");
    strcpy(st.outfitSlotText, "1");
    st.stage_days = 0;
    for (size_t i = 0; i < PetStatSnapshot::kCustomStatCount; ++i)
        st.customStats[i] = 0;
    st.flowFlags = 0;
    st.outfitUnlockMask = 0;
    st.crc32 = 0;
}

void Pet::setSchemaFingerprint(uint32_t fingerprint)
{
    st.schemaFingerprint = fingerprint;
}

const PersistedPetState &Pet::persistentState() const
{
    return st;
}

uint8_t Pet::speciesSlot() const
{
    return parseAppearanceSlot(st.speciesSlotText);
}

uint8_t Pet::outfitSlot() const
{
    return parseAppearanceSlot(st.outfitSlotText);
}

uint32_t Pet::stageDays() const
{
    return st.stage_days;
}

PetStatSnapshot Pet::statSnapshot() const
{
    PetStatSnapshot snapshot = {};
    snapshot.stage_days = st.stage_days;
    snapshot.speciesSlot = speciesSlot();
    snapshot.outfitSlot = outfitSlot();
    for (size_t i = 0; i < PetStatSnapshot::kCustomStatCount; ++i)
        snapshot.customStats[i] = st.customStats[i];
    return snapshot;
}

int16_t Pet::customStat(uint8_t index) const
{
    if (index >= PetStatSnapshot::kCustomStatCount)
        return 0;

    return st.customStats[index];
}

bool Pet::setCustomStat(uint8_t index, int16_t value)
{
    if (index >= PetStatSnapshot::kCustomStatCount)
        return false;

    st.customStats[index] = value;
    return true;
}

bool Pet::changeCustomStat(uint8_t index, int16_t delta)
{
    if (index >= PetStatSnapshot::kCustomStatCount)
        return false;

    st.customStats[index] = static_cast<int16_t>(st.customStats[index] + delta);
    return true;
}

bool Pet::changeCustomStatClamped(uint8_t index, int16_t delta, int16_t minValue, int16_t maxValue)
{
    if (index >= PetStatSnapshot::kCustomStatCount || minValue > maxValue)
        return false;

    const int32_t next = static_cast<int32_t>(st.customStats[index]) + delta;
    st.customStats[index] = static_cast<int16_t>(clampValue<int32_t>(next, minValue, maxValue));
    return true;
}

bool Pet::setSpeciesSlot(uint8_t slot)
{
    char text[sizeof(st.speciesSlotText)] = {};
    if (!writeAppearanceSlot(text, sizeof(text), slot))
        return false;
    if (strcmp(st.speciesSlotText, text) != 0)
    {
        strcpy(st.speciesSlotText, text);
        st.stage_days = 0;
        st.outfitUnlockMask = 0;
    }
    return true;
}

bool Pet::setOutfitSlot(uint8_t slot)
{
    char text[sizeof(st.outfitSlotText)] = {};
    if (!writeAppearanceSlot(text, sizeof(text), slot))
        return false;
    strcpy(st.outfitSlotText, text);
    return true;
}

uint8_t Pet::outfitUnlockMask() const { return st.outfitUnlockMask; }

bool Pet::isOutfitUnlocked(uint8_t slot) const
{
    return slot >= 1 && slot <= 8 && (st.outfitUnlockMask & (1U << (slot - 1U))) != 0;
}

void Pet::unlockOutfit(uint8_t slot)
{
    if (slot >= 1 && slot <= 8)
        st.outfitUnlockMask |= static_cast<uint8_t>(1U << (slot - 1U));
}

void Pet::initializeOutfitUnlockMask(uint8_t mask)
{
    st.outfitUnlockMask = mask;
}

bool Pet::isFirstLaunchComplete() const
{
    return (st.flowFlags & kFirstLaunchCompleteFlag) != 0;
}

void Pet::markFirstLaunchComplete()
{
    st.flowFlags |= kFirstLaunchCompleteFlag;
}

void Pet::resetFirstLaunch()
{
    st.flowFlags &= ~kFirstLaunchCompleteFlag;
}

bool Pet::isFirstStartCompleted() const
{
    return (st.flowFlags & kFirstStartCompletedFlag) != 0;
}

void Pet::markFirstStartCompleted()
{
    st.flowFlags |= kFirstStartCompletedFlag;
}

void Pet::resetFirstStartCompleted()
{
    st.flowFlags &= ~kFirstStartCompletedFlag;
}

bool Pet::restoreState(const PersistedPetState &state)
{
    if (state.magic != kPetStateMagic || state.version != kPetStateVersion)
        return false;

    st = state;
    st.version = kPetStateVersion;
    char appearanceCode[9] = {};
    if (!copyPersistedAppearanceText(appearanceCode, sizeof(appearanceCode), st.speciesSlotText))
        strcpy(st.speciesSlotText, "dino");
    if (!copyPersistedAppearanceText(appearanceCode, sizeof(appearanceCode), st.outfitSlotText))
        strcpy(st.outfitSlotText, "base");

    return true;
}
