#include "execution/Pipeline.h"

#include <windows.h>

namespace kshell
{

std::optional<Pipeline::BuildPlan> Pipeline::build(size_t commandCount, bool background,
                                                   const std::vector<RedirectionPlan>& redirectPlans)
{
    BuildPlan plan;
    plan.commandCount = commandCount;
    plan.background = background;
    plan.redirectPlans = redirectPlans;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    for (size_t i = 0; i + 1 < commandCount; ++i)
    {
        HANDLE readHandle = nullptr;
        HANDLE writeHandle = nullptr;
        if (!::CreatePipe(&readHandle, &writeHandle, &sa, 0))
        {
            return std::nullopt;
        }
        plan.readPipeHandles.emplace_back(readHandle);
        plan.writePipeHandles.emplace_back(writeHandle);
    }

    return plan;
}

} // namespace kshell
