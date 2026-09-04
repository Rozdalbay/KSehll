#pragma once

#ifndef KSHELL_PIPELINE_H
#define KSHELL_PIPELINE_H

#include <vector>
#include <optional>

#include "utils/WinHandle.h"
#include "execution/Process.h"
#include "execution/Redirection.h"

namespace kshell
{

class PipelineManager
{
public:
    struct BuildPlan
    {
        size_t commandCount = 0;
        bool background = false;
        std::vector<WinHandle> readPipeHandles;
        std::vector<WinHandle> writePipeHandles;
        std::vector<ProcessHandle> processes;
        std::vector<RedirectionPlan> redirectPlans;
    };

    static std::optional<BuildPlan> build(size_t commandCount, bool background,
                                          const std::vector<RedirectionPlan>& redirectPlans);
};

} // namespace kshell

#endif // KSHELL_PIPELINE_H
