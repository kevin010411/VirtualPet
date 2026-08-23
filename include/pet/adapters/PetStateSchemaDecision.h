#ifndef PET_STATE_SCHEMA_DECISION_H
#define PET_STATE_SCHEMA_DECISION_H

#include <stdint.h>

enum class PetStateSchemaDecision : uint8_t
{
    Restore,
    Reset,
};

inline PetStateSchemaDecision decidePetStateSchema(
    uint32_t savedSchemaFingerprint,
    uint32_t exportedSchemaFingerprint)
{
    return savedSchemaFingerprint == exportedSchemaFingerprint
               ? PetStateSchemaDecision::Restore
               : PetStateSchemaDecision::Reset;
}

#endif // PET_STATE_SCHEMA_DECISION_H
