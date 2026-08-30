#ifndef SYSTEM_COMMAND_CATALOG_H
#define SYSTEM_COMMAND_CATALOG_H

#include <stdint.h>
#include "shared/config/AppProfile.h"

enum class SystemCommandHandler : uint8_t
{
#define SYSTEM_COMMAND(handler, token, runtimeId, slot) handler,
#include "commands/domain/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
};

enum class RuntimeSystemCommandId : uint16_t
{
    Predict = 0,
    GuessGame = 1,
    Status = 2,
    ChangeOutfit = 3,
    ChangeSpecies = 4,
};

struct CompiledSystemCommand
{
    const char *token;
    SystemCommandHandler handler;
    RuntimeSystemCommandId runtimeId;
};

// Returns null when SD content names a command without a compiled handler.
const CompiledSystemCommand *findCompiledSystemCommand(const char *token);
const CompiledSystemCommand *findCompiledSystemCommand(RuntimeSystemCommandId id);
bool runtimeSystemCommandIdForToken(const char *token, RuntimeSystemCommandId &id);

#endif // SYSTEM_COMMAND_CATALOG_H
