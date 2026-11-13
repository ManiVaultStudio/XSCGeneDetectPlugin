#pragma once

#include <QThread>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#include <fstream>
#include <string>
#endif

namespace ExpressionScanResources
{
inline constexpr std::size_t MiB = 1024 * 1024;

inline std::optional<std::uint64_t> availableMemoryBytes()
{
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return std::min(status.ullAvailPhys, status.ullAvailPageFile);
#elif defined(Q_OS_MACOS)
    vm_statistics64_data_t statistics{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const auto host = mach_host_self();
    const auto result = host_statistics64(host, HOST_VM_INFO64,
        reinterpret_cast<host_info64_t>(&statistics), &count);
    mach_port_deallocate(mach_task_self(), host);
    if (result == KERN_SUCCESS)
        return (std::uint64_t(statistics.free_count) + statistics.inactive_count) * vm_kernel_page_size;
#elif defined(Q_OS_LINUX)
    std::ifstream memoryInfo("/proc/meminfo");
    for (std::string key; memoryInfo >> key;) {
        if (key == "MemAvailable:") {
            std::uint64_t kibibytes = 0;
            if (memoryInfo >> kibibytes && kibibytes <= std::numeric_limits<std::uint64_t>::max() / 1024)
                return kibibytes * 1024;
            break;
        }
        memoryInfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
#endif
    return std::nullopt;
}

inline std::size_t memoryBudget(int configuredMegabytes)
{
    const auto ceiling = static_cast<std::uint64_t>(std::max(1, configuredMegabytes)) * MiB;
    // Leave at least three quarters of the reported available memory for the
    // rest of the application, concurrent work, and the filesystem cache.
    const auto available = availableMemoryBytes();
    const auto budget = available ? std::min(ceiling, *available / 4) : std::min<std::uint64_t>(ceiling, 512 * MiB);
    return static_cast<std::size_t>(std::min<std::uint64_t>(budget, std::numeric_limits<std::size_t>::max()));
}

inline int cpuLimit(int configuredWorkers)
{
    return std::max(1, std::min(configuredWorkers, QThread::idealThreadCount()));
}

inline int workersForMemory(int limit, std::size_t budget, std::size_t sharedBytes, std::size_t bytesPerWorker)
{
    const auto available = budget > sharedBytes ? budget - sharedBytes : 0;
    const auto workers = std::min(static_cast<std::size_t>(std::max(1, limit)),
        available / std::max<std::size_t>(1, bytesPerWorker));
    return static_cast<int>(std::max<std::size_t>(1, workers));
}
}
