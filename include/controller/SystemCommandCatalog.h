#ifndef SYSTEM_COMMAND_CATALOG_H
#define SYSTEM_COMMAND_CATALOG_H

#include <stdint.h>
#include "common/AppProfile.h"

enum class SystemCommandHandler : uint8_t
{
#define SYSTEM_COMMAND(handler, runtimeId, slot) handler,
#include "controller/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
};

enum class RuntimeSystemCommandId : uint16_t
{
    Predict = 0,
    GuessGame = 1,
    Status = 2,
    ChangeOutfit = 3,
    ChangeSpecies = 4,
    CollectionCatalog = 5,
    InstructionResponse = 6,
};

struct CompiledSystemCommand
{
    SystemCommandHandler handler;
    RuntimeSystemCommandId runtimeId;
};

// Returns null when the runtime ID has no compiled handler.
const CompiledSystemCommand *findCompiledSystemCommand(RuntimeSystemCommandId id);

#endif // SYSTEM_COMMAND_CATALOG_H
