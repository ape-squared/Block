#pragma once

#include "input_manager.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace vkShade
{
    class InputBackendWin32 : public InputManager
    {
    public:
        explicit InputBackendWin32(HWND hwnd);
        ~InputBackendWin32() override = default;

        void process_events() override;

    private:
        HWND m_hwnd;

        glm::vec2 m_prevMousePos {0.0f, 0.0f};
        bool m_prevLeftButton = false;
        bool m_prevMiddleButton = false;
        bool m_prevRightButton = false;

        static KeyCode map_virtual_key(int vkey);
        void poll_keyboard();
        void poll_mouse();
        bool is_target_foreground() const;
    };
} // namespace vkShade
