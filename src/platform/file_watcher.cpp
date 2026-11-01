#include "file_watcher.hpp"

#include <memory>
#include <system_error>

#include "core/logger.hpp"

#if defined(_WIN32)
    #include "windows/file_watcher.hpp"
    using FileWatcherImpl = vkShade::Platform::WindowsFileWatcher;
#elif defined(__linux__)
    #include "linux/file_watcher.hpp"
    using FileWatcherImpl = vkShade::Platform::LinuxFileWatcher;
#else
    #error "Unsupported platform"
#endif

namespace vkShade::Platform
{
    std::unique_ptr<FileWatcher> FileWatcher::create()
    {
        try {
            return std::make_unique<FileWatcherImpl>();
        }
        catch (const std::system_error& e) {
            Logger::error("Failed to create file watcher: {}", e.what());
            return nullptr;
        }
    }
} // namespace vkShade::Platform
