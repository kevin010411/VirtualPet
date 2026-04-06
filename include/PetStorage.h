#ifndef PET_STORAGE_H
#define PET_STORAGE_H

#include <SdFat.h>
#include "Pet.h"

class PetStorage
{
public:
    explicit PetStorage(SdFat *ref_sd);

    bool save(const Pet &pet);
    bool load(Pet &pet);

private:
    SdFat *sd;
};

#endif
