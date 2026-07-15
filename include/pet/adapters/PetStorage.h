#ifndef PET_STORAGE_H
#define PET_STORAGE_H

#include <SdFat.h>
#include "pet/domain/Pet.h"

class PetStorage
{
public:
    explicit PetStorage(SdFat *ref_sd);

    bool save(const Pet &pet);
    bool load(Pet &pet);
    char lastSaveSlot() const;
    uint32_t lastSaveSequence() const;

private:
    SdFat *sd;
    uint32_t nextSequence = 1;
    bool writeSlotA = true;
    char attemptedSaveSlot = 'A';
    uint32_t attemptedSaveSequence = 0;
};

#endif
