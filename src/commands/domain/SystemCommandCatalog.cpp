#include "commands/domain/SystemCommandCatalog.h"

#include <stddef.h>
#include <string.h>
#include "shared/config/AppProfile.h"

namespace
{
constexpr CompiledSystemCommand kCompiledSystemCommands[] = {
#define SYSTEM_COMMAND(handler, token, runtimeId, slot) \
    {token, SystemCommandHandler::handler, RuntimeSystemCommandId::runtimeId},
#include "commands/domain/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
};
} // namespace

const CompiledSystemCommand *findCompiledSystemCommand(const char *token)
{
    if (token == nullptr)
        return nullptr;

    for (const CompiledSystemCommand &command : kCompiledSystemCommands)
    {
        if (strcmp(token, command.token) == 0)
            return &command;
    }
    return nullptr;
}

const CompiledSystemCommand *findCompiledSystemCommand(RuntimeSystemCommandId id)
{
    for (const CompiledSystemCommand &command : kCompiledSystemCommands)
        if (command.runtimeId == id)
            return &command;
    return nullptr;
}

bool runtimeSystemCommandIdForToken(const char *token, RuntimeSystemCommandId &id)
{
    const CompiledSystemCommand *command = findCompiledSystemCommand(token);
    if (command == nullptr)
        return false;
    id = command->runtimeId;
    return true;
}
