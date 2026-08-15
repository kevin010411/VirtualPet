#ifndef SYSTEM_COMMAND_CATALOG_H
#define SYSTEM_COMMAND_CATALOG_H

#include <stdint.h>
#include "shared/config/AppProfile.h"

enum class SystemCommandHandler : uint8_t
{
#define SYSTEM_COMMAND(handler, token, slot) handler,
#include "commands/domain/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
};

struct CompiledSystemCommand
{
    const char *token;
    SystemCommandHandler handler;
};

// Returns null when SD content names a command without a compiled handler.
const CompiledSystemCommand *findCompiledSystemCommand(const char *token);

#endif // SYSTEM_COMMAND_CATALOG_H
