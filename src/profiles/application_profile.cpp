#include "application_profile.hpp"

#include "core/logger.hpp"

vkShade::ProfileRegistry::ProfileRegistry()
{
    m_profiles.push_back(std::make_unique<SecondLifeProfile>());
    m_profiles.push_back(std::make_unique<CustomProfile>("Custom"));
    m_profiles.push_back(std::make_unique<GenericVulkanProfile>());
    m_active = m_profiles.back().get();
}

void vkShade::ProfileRegistry::resolve()
{
    for (auto& profile : m_profiles)
    {
        if (!profile->detect())
            continue;

        if (!profile->initialize())
        {
            Logger::warn("Profile '{}' detected but failed to initialize", profile->name());
            continue;
        }

        m_active = profile.get();
        Logger::info("Active application profile: {}", m_active->name());
        return;
    }

    m_active = m_profiles.back().get();
    Logger::info("Active application profile: {} (fallback)", m_active->name());
}
