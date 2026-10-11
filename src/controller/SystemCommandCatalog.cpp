#include "controller/SystemCommandCatalog.h"

#include "common/AppProfile.h"

namespace
{
constexpr CompiledSystemCommand kCompiledSystemCommands[] = {
#define SYSTEM_COMMAND(handler, runtimeId, slot) \
    {SystemCommandHandler::handler, RuntimeSystemCommandId::runtimeId},
#include "controller/SystemCommandCatalog.def"
#undef SYSTEM_COMMAND
};
} // namespace

const CompiledSystemCommand *findCompiledSystemCommand(RuntimeSystemCommandId id)
{
    for (const CompiledSystemCommand &command : kCompiledSystemCommands)
        if (command.runtimeId == id)
            return &command;
    return nullptr;
}
