#pragma once

#include <cstdint>

namespace vkShade
{
    // Indices match ReShade mousebutton keycode annotations (0..4).
    enum class MouseButton : uint32_t
    {
        LEFT = 0,
        RIGHT = 1,
        MIDDLE = 2,
        X1 = 3,
        X2 = 4,
    };
} // namespace vkShade
