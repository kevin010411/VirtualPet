#include "pet/Pet.h"

#include <string.h>
#include "common/CanonicalDecimal.h"

namespace
{
constexpr uint32_t kFirstLaunchCompleteFlag = 0x1UL;
constexpr uint32_t kFirstStartCompletedFlag = 0x2UL;
constexpr uint32_t kDeadFlag = 0x4UL;

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

bool Pet::isDead() const { return (st.flowFlags & kDeadFlag) != 0; }
#if ENABLE_DEATH
void Pet::configureDeath(bool enabled, uint16_t statMask)
{
    deathStatMask = enabled ? statMask : 0;
}
bool Pet::evaluateDeath()
{
    if (!isDead())
        for (uint8_t slot = 0; slot < kPetCustomStatCount; ++slot)
            if ((deathStatMask & (1U << slot)) != 0 && st.customStats[slot] <= 0)
            { st.flowFlags |= kDeadFlag; break; }
    return isDead();
}
#endif

bool Pet::configureTrackedActions(uint32_t fingerprint)
{
    if (st.trackedActionFingerprint == fingerprint) return false;
    st.trackedActionFingerprint = fingerprint;
    resetActionCounts();
    return true;
}

void Pet::resetActionCounts() { memset(st.actionCounts, 0, sizeof(st.actionCounts)); }
uint16_t Pet::actionCount(uint8_t slot) const { return slot < kTrackedActionCount ? st.actionCounts[slot] : 0; }
void Pet::incrementActionCount(uint8_t slot)
{
    if (slot < kTrackedActionCount && st.actionCounts[slot] < UINT16_MAX) ++st.actionCounts[slot];
}

bool Pet::configureCollection(uint32_t fingerprint)
{
    if (st.collectionFingerprint == fingerprint)
        return false;
    st.collectionFingerprint = fingerprint;
    st.collectionMask = 0;
    return true;
}

bool Pet::isCollectionOwned(uint8_t slot) const
{
    return slot < 64 && (st.collectionMask & (uint64_t(1) << slot)) != 0;
}

void Pet::acquireCollection(uint8_t slot)
{
    if (slot < 64) st.collectionMask |= uint64_t(1) << slot;
}

bool Pet::commitPetDay(const int16_t *customStats, size_t customStatCount, uint32_t days)
{
    if (isDead()) return false;
    if (customStats == nullptr || customStatCount == 0 || customStatCount > kPetCustomStatCount)
        return false;

    const uint32_t remaining = st.stage_days < kStageDaysMax ? kStageDaysMax - st.stage_days : 0;
    const uint32_t nextStageDays = days >= remaining ? kStageDaysMax : st.stage_days + days;
    st.stage_days = nextStageDays;
    return commitPetStats(customStats, customStatCount);
}

bool Pet::commitPetStats(const int16_t *customStats, size_t customStatCount)
{
    if (isDead()) return false;
    if (customStats == nullptr || customStatCount == 0 || customStatCount > kPetCustomStatCount)
        return false;

    for (size_t index = 0; index < customStatCount; ++index)
        st.customStats[index] = customStats[index];
    evaluateDeath();
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

void Pet::setStageDays(uint32_t value)
{
    st.stage_days = value > kStageDaysMax ? kStageDaysMax : value;
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
    if (isDead()) return false;
    if (index >= PetStatSnapshot::kCustomStatCount)
        return false;

    st.customStats[index] = value;
    evaluateDeath();
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
        resetActionCounts();
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
    if (strcmp(st.outfitSlotText, text) != 0) resetActionCounts();
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

bool Pet::commitConsumableOutfitUnlock(uint8_t outfitSlot,
                                       const PetStatSnapshot &consumedStats)
{
    if (isDead()) return false;
    if (outfitSlot == 0 || outfitSlot > 8 ||
        consumedStats.speciesSlot != speciesSlot() ||
        consumedStats.outfitSlot != this->outfitSlot() ||
        consumedStats.stage_days > st.stage_days)
        return false;
    for (size_t index = 0; index < kPetCustomStatCount; ++index)
        if (consumedStats.customStats[index] > st.customStats[index])
            return false;
    st.stage_days = consumedStats.stage_days;
    memcpy(st.customStats, consumedStats.customStats, sizeof(st.customStats));
    st.outfitUnlockMask |= static_cast<uint8_t>(1U << (outfitSlot - 1U));
    setOutfitSlot(outfitSlot);
    evaluateDeath();
    return true;
}

bool Pet::isFirstLaunchComplete() const
{
    return (st.flowFlags & kFirstLaunchCompleteFlag) != 0;
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
    if (state.magic != kPetStateMagic || state.version != kPetStateVersion ||
        parseAppearanceSlot(state.speciesSlotText) == 0 ||
        parseAppearanceSlot(state.outfitSlotText) == 0)
        return false;

    st = state;
    st.version = kPetStateVersion;
    if (st.stage_days > kStageDaysMax)
        st.stage_days = kStageDaysMax;
    return true;
}
