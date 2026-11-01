#pragma once

#include <string>

namespace vkShade::Events
{
    // Empty effect name reloads all effects; otherwise only the named file.
    struct ReloadEffects
    {
        std::string effect;
    };
} // namespace vkShade::Events
