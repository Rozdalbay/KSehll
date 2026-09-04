#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kshell::system
{

struct SystemStats
{
    double cpuPercent = 0.0;
    double ramPercent = 0.0;
    uint64_t ramUsedBytes = 0;
    uint64_t ramTotalBytes = 0;
    double diskPercent = 0.0;
    uint64_t diskFreeBytes = 0;
    uint64_t diskTotalBytes = 0;
    uint64_t processCount = 0;
    uint64_t uptimeSeconds = 0;
    // Recent CPU samples for a mini sparkline (older -> newer).
    std::vector<double> cpuHistory;
    std::vector<double> ramHistory;
};

// Lightweight, non-blocking system metrics. Designed to be refreshed by a
// background timer (async) so the render loop never sleeps or polls hard.
class SystemMonitor
{
public:
    SystemMonitor();

    // Refresh all metrics. Safe to call from a non-UI worker.
    SystemStats refresh();

private:
    double cpuRatio();
    uint64_t previousSystem_ = 0;
    uint64_t previousIdle_ = 0;
    bool     primed_ = false;
    // Persistent sample history so a real-time graph can span many refreshes.
    std::vector<double> cpuHistory_;
    std::vector<double> ramHistory_;
};

} // namespace kshell::system
