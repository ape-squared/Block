#include "config_manager.hpp"

#include "core/logger.hpp"
#include "platform/file_watcher.hpp"

#include "config_globals.hpp"

#include <array>
#include <cstdlib>

namespace
{
    std::string env_or_empty(const char* name)
    {
        const char* value = std::getenv(name);
        return value ? std::string(value) : std::string();
    }

#if defined(_WIN32)
    std::string win_appdata_block()
    {
        const std::string base = env_or_empty("APPDATA");
        return base.empty() ? std::string() : base + "\\Block";
    }

    std::string win_localappdata_block()
    {
        const std::string base = env_or_empty("LOCALAPPDATA");
        return base.empty() ? std::string() : base + "\\Block";
    }
#endif
} // namespace

vkShade::ConfigManager::ConfigManager()
    : m_config(ConfigStore(ConfigStore::Type::Config)),
      m_internal(ConfigStore(ConfigStore::Type::Internal)),
      m_preset(ConfigStore(ConfigStore::Type::Preset))
{
    load_config_file();
    load_preset_file();
}

void vkShade::ConfigManager::load_config_file()
{
    std::string customConfigFile = env_or_empty("BLOCK_CONFIG_FILE");
    if (customConfigFile.empty())
        customConfigFile = env_or_empty("VKSHADE_CONFIG_FILE");

#if defined(_WIN32)
    const std::string userRoaming = win_appdata_block();
    const std::string userLocal = win_localappdata_block();

    const std::array<std::string, 8> configCandidates = {
        customConfigFile,
        "Block.ini",
        "vkShade.ini",
        userRoaming.empty() ? std::string() : userRoaming + "\\Block.ini",
        userLocal.empty() ? std::string() : userLocal + "\\Block.ini",
        userRoaming.empty() ? std::string() : userRoaming + "\\vkShade.ini",
        std::string(SYSCONFDIR) + "/Block/Block.ini",
        std::string(DATADIR) + "/Block/Block.ini",
    };
#else
    const char* xdgDataEnv = std::getenv("XDG_DATA_HOME");
    std::string userDefaultConfigFile = xdgDataEnv ? std::string(xdgDataEnv) + "/Block/Block.ini"
                                                   : std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.local/share/Block/Block.ini";

    const char* xdgConfigEnv = std::getenv("XDG_CONFIG_HOME");
    std::string userCustomConfigFile = xdgConfigEnv ? std::string(xdgConfigEnv) + "/Block/Block.ini"
                                                    : std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.config/Block/Block.ini";

    // Prefer Block paths; keep vkShade paths as migration fallbacks.
    const std::array<std::string, 9> configCandidates = {
        customConfigFile,
        "Block.ini",
        "vkShade.ini",
        userCustomConfigFile,
        userDefaultConfigFile,
        std::string(SYSCONFDIR) + "/Block.ini",
        std::string(SYSCONFDIR) + "/Block/Block.ini",
        std::string(DATADIR) + "/Block/Block.ini",
        std::string(DATADIR) + "/vkShade/vkShade.ini",
    };
#endif

    for (const auto& filePath : configCandidates)
    {
        if (filePath.empty())
            continue;
        if (m_config.load(filePath))
            return;
    }

    Logger::warn("No config file found");
}

void vkShade::ConfigManager::load_preset_file()
{
    std::string customPresetFile = env_or_empty("BLOCK_PRESET_FILE");
    if (customPresetFile.empty())
        customPresetFile = env_or_empty("VKSHADE_PRESET_FILE");

#if defined(_WIN32)
    const std::string userRoaming = win_appdata_block();
    const std::string userLocal = win_localappdata_block();

    const std::array<std::string, 7> presetCandidates = {
        customPresetFile,
        "ReShade.ini",
        userRoaming.empty() ? std::string() : userRoaming + "\\ReShade.ini",
        userLocal.empty() ? std::string() : userLocal + "\\ReShade.ini",
        std::string(SYSCONFDIR) + "/Block/ReShade.ini",
        std::string(DATADIR) + "/Block/ReShade.ini",
        std::string(DATADIR) + "/vkShade/ReShade.ini",
    };
#else
    const char* xdgDataEnv = std::getenv("XDG_DATA_HOME");
    std::string userDefaultPresetFile = xdgDataEnv ? std::string(xdgDataEnv) + "/Block/ReShade.ini"
                                                   : std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.local/share/Block/ReShade.ini";

    const char* xdgPresetEnv = std::getenv("XDG_CONFIG_HOME");
    std::string userCustomPresetFile = xdgPresetEnv ? std::string(xdgPresetEnv) + "/Block/ReShade.ini"
                                                    : std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.config/Block/ReShade.ini";

    const std::array<std::string, 8> presetCandidates = {
        customPresetFile,
        "ReShade.ini",
        userCustomPresetFile,
        userDefaultPresetFile,
        std::string(SYSCONFDIR) + "/ReShade.ini",
        std::string(SYSCONFDIR) + "/Block/ReShade.ini",
        std::string(DATADIR) + "/Block/ReShade.ini",
        std::string(DATADIR) + "/vkShade/ReShade.ini",
    };
#endif

    for (const auto& filePath : presetCandidates)
    {
        if (filePath.empty())
            continue;
        if (m_preset.load(filePath))
            return;
    }

    Logger::warn("No preset file found");
}

void vkShade::ConfigManager::update()
{
    m_config.update();
    m_preset.update();
}
