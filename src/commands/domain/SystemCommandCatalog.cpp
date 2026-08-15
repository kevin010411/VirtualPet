#include "commands/domain/SystemCommandCatalog.h"

#include <stddef.h>
#include <string.h>
#include "shared/config/AppProfile.h"

namespace
{
constexpr CompiledSystemCommand kCompiledSystemCommands[] = {
#define SYSTEM_COMMAND(handler, token, slot) {token, SystemCommandHandler::handler},
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
