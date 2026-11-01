#pragma once

#include "platform/file_watcher.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "core/logger.hpp"

namespace vkShade::Platform
{
    class WindowsFileWatcher final : public FileWatcher
    {
    public:
        WindowsFileWatcher() = default;

        ~WindowsFileWatcher() override
        {
            unwatch();
        }

        bool watch(const std::filesystem::path& path) override
        {
            unwatch();

            m_path = std::filesystem::absolute(path);
            const auto parent = m_path.parent_path();

            m_changeHandle = FindFirstChangeNotificationW(
                parent.c_str(),
                FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);

            if (m_changeHandle == INVALID_HANDLE_VALUE)
            {
                Logger::error("FindFirstChangeNotification failed ({})", GetLastError());
                m_changeHandle = nullptr;
                return false;
            }

            std::error_code ec;
            if (std::filesystem::exists(m_path, ec))
                m_lastWrite = std::filesystem::last_write_time(m_path, ec);

            Logger::debug("Watching for changes: {}", m_path.string());
            return true;
        }

        void unwatch() override
        {
            if (m_changeHandle)
            {
                FindCloseChangeNotification(m_changeHandle);
                m_changeHandle = nullptr;
                Logger::debug("Stopped watching: {}", m_path.string());
                m_path.clear();
            }
        }

        bool changed() override
        {
            if (!m_changeHandle)
                return false;

            bool directorySignaled = false;
            if (WaitForSingleObject(m_changeHandle, 0) == WAIT_OBJECT_0)
            {
                directorySignaled = true;
                FindNextChangeNotification(m_changeHandle);
            }

            if (!directorySignaled)
                return false;

            std::error_code ec;
            if (!std::filesystem::exists(m_path, ec))
                return true;

            const auto writeTime = std::filesystem::last_write_time(m_path, ec);
            if (ec)
                return true;

            if (writeTime != m_lastWrite)
            {
                m_lastWrite = writeTime;
                return true;
            }

            return false;
        }

    private:
        HANDLE m_changeHandle {nullptr};
        std::filesystem::path m_path;
        std::filesystem::file_time_type m_lastWrite {};
    };
} // namespace vkShade::Platform
