#include "input_backend_win32.hpp"

#include "core/logger.hpp"

vkShade::InputBackendWin32::InputBackendWin32(HWND hwnd)
    : m_hwnd(hwnd)
{
    Logger::debug("Initializing Win32 input backend");
}

bool vkShade::InputBackendWin32::is_target_foreground() const
{
    if (!m_hwnd || !IsWindow(m_hwnd))
        return false;

    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    return foreground == m_hwnd || IsChild(foreground, m_hwnd) || IsChild(m_hwnd, foreground);
}

vkShade::KeyCode vkShade::InputBackendWin32::map_virtual_key(int vkey)
{
    if (vkey >= '0' && vkey <= '9')
        return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KEY_0) + (vkey - '0'));
    if (vkey >= 'A' && vkey <= 'Z')
        return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KEY_A) + (vkey - 'A'));

    switch (vkey)
    {
        case VK_OEM_3:       return KeyCode::KEY_BACKQUOTE;
        case VK_TAB:         return KeyCode::KEY_TAB;
        case VK_LSHIFT:      return KeyCode::KEY_LSHIFT;
        case VK_RSHIFT:      return KeyCode::KEY_RSHIFT;
        case VK_LCONTROL:    return KeyCode::KEY_LCTRL;
        case VK_RCONTROL:    return KeyCode::KEY_RCTRL;
        case VK_LMENU:       return KeyCode::KEY_LALT;
        case VK_RMENU:       return KeyCode::KEY_RALT;
        case VK_SPACE:       return KeyCode::KEY_SPACE;
        case VK_ESCAPE:      return KeyCode::KEY_ESCAPE;
        case VK_BACK:        return KeyCode::KEY_BACKSPACE;
        case VK_RETURN:      return KeyCode::KEY_RETURN;
        case VK_OEM_PLUS:    return KeyCode::KEY_EQUALS;
        case VK_OEM_MINUS:   return KeyCode::KEY_MINUS;
        case VK_OEM_4:       return KeyCode::KEY_LEFTBRACKET;
        case VK_OEM_6:       return KeyCode::KEY_RIGHTBRACKET;
        case VK_OEM_5:       return KeyCode::KEY_BACKSLASH;
        case VK_OEM_1:       return KeyCode::KEY_SEMICOLON;
        case VK_OEM_7:       return KeyCode::KEY_QUOTE;
        case VK_OEM_2:       return KeyCode::KEY_SLASH;
        case VK_OEM_COMMA:   return KeyCode::KEY_COMMA;
        case VK_OEM_PERIOD:  return KeyCode::KEY_PERIOD;
        case VK_INSERT:      return KeyCode::KEY_INSERT;
        case VK_DELETE:      return KeyCode::KEY_DELETE;
        case VK_HOME:        return KeyCode::KEY_HOME;
        case VK_END:         return KeyCode::KEY_END;
        case VK_PRIOR:       return KeyCode::KEY_PAGEUP;
        case VK_NEXT:        return KeyCode::KEY_PAGEDOWN;
        case VK_SNAPSHOT:    return KeyCode::KEY_PRINTSCREEN;
        case VK_SCROLL:      return KeyCode::KEY_SCROLLLOCK;
        case VK_PAUSE:       return KeyCode::KEY_PAUSE;
        case VK_UP:          return KeyCode::KEY_UP;
        case VK_DOWN:        return KeyCode::KEY_DOWN;
        case VK_LEFT:        return KeyCode::KEY_LEFT;
        case VK_RIGHT:       return KeyCode::KEY_RIGHT;
        case VK_ADD:         return KeyCode::KEY_NUMPAD_PLUS;
        case VK_SUBTRACT:    return KeyCode::KEY_NUMPAD_MINUS;
        case VK_MULTIPLY:    return KeyCode::KEY_NUMPAD_MULTIPLY;
        case VK_DIVIDE:      return KeyCode::KEY_NUMPAD_DIVIDE;
        case VK_DECIMAL:     return KeyCode::KEY_NUMPAD_PERIOD;
        case VK_NUMPAD0:     return KeyCode::KEY_NUMPAD_0;
        case VK_NUMPAD1:     return KeyCode::KEY_NUMPAD_1;
        case VK_NUMPAD2:     return KeyCode::KEY_NUMPAD_2;
        case VK_NUMPAD3:     return KeyCode::KEY_NUMPAD_3;
        case VK_NUMPAD4:     return KeyCode::KEY_NUMPAD_4;
        case VK_NUMPAD5:     return KeyCode::KEY_NUMPAD_5;
        case VK_NUMPAD6:     return KeyCode::KEY_NUMPAD_6;
        case VK_NUMPAD7:     return KeyCode::KEY_NUMPAD_7;
        case VK_NUMPAD8:     return KeyCode::KEY_NUMPAD_8;
        case VK_NUMPAD9:     return KeyCode::KEY_NUMPAD_9;
        case VK_F1:          return KeyCode::KEY_F1;
        case VK_F2:          return KeyCode::KEY_F2;
        case VK_F3:          return KeyCode::KEY_F3;
        case VK_F4:          return KeyCode::KEY_F4;
        case VK_F5:          return KeyCode::KEY_F5;
        case VK_F6:          return KeyCode::KEY_F6;
        case VK_F7:          return KeyCode::KEY_F7;
        case VK_F8:          return KeyCode::KEY_F8;
        case VK_F9:          return KeyCode::KEY_F9;
        case VK_F10:         return KeyCode::KEY_F10;
        case VK_F11:         return KeyCode::KEY_F11;
        case VK_F12:         return KeyCode::KEY_F12;
        case VK_SHIFT:       return KeyCode::KEY_LSHIFT;
        case VK_CONTROL:     return KeyCode::KEY_LCTRL;
        case VK_MENU:        return KeyCode::KEY_LALT;
        default:             return KeyCode::KEY_UNKNOWN;
    }
}

void vkShade::InputBackendWin32::poll_keyboard()
{
    static const int kKeys[] = {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        VK_OEM_3, VK_TAB, VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL,
        VK_LMENU, VK_RMENU, VK_SPACE, VK_ESCAPE, VK_BACK, VK_RETURN,
        VK_OEM_PLUS, VK_OEM_MINUS, VK_OEM_4, VK_OEM_6, VK_OEM_5, VK_OEM_1,
        VK_OEM_7, VK_OEM_2, VK_OEM_COMMA, VK_OEM_PERIOD,
        VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
        VK_SNAPSHOT, VK_SCROLL, VK_PAUSE,
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
        VK_ADD, VK_SUBTRACT, VK_MULTIPLY, VK_DIVIDE, VK_DECIMAL,
        VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4,
        VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9,
        VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
        VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
    };

    const bool active = is_target_foreground();

    for (int vkey : kKeys)
    {
        const KeyCode code = map_virtual_key(vkey);
        if (code == KeyCode::KEY_UNKNOWN)
            continue;

        const bool down = active && ((GetAsyncKeyState(vkey) & 0x8000) != 0);
        const bool wasDown = m_currentKeyStates[code];
        if (down != wasDown)
            handle_key_code_event(code, down);
    }
}

void vkShade::InputBackendWin32::poll_mouse()
{
    if (!is_target_foreground())
        return;

    POINT pt {};
    if (!GetCursorPos(&pt))
        return;
    if (!ScreenToClient(m_hwnd, &pt))
        return;

    const glm::vec2 pos {static_cast<float>(pt.x), static_cast<float>(pt.y)};
    if (pos != m_prevMousePos)
    {
        handle_mouse_motion_event(pos.x, pos.y);
        m_prevMousePos = pos;
    }

    const bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool middle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    if (left != m_prevLeftButton)
        handle_mouse_button_event(MouseButton::LEFT, left);
    if (middle != m_prevMiddleButton)
        handle_mouse_button_event(MouseButton::MIDDLE, middle);
    if (right != m_prevRightButton)
        handle_mouse_button_event(MouseButton::RIGHT, right);

    m_prevLeftButton = left;
    m_prevMiddleButton = middle;
    m_prevRightButton = right;
}

void vkShade::InputBackendWin32::process_events()
{
    poll_keyboard();
    poll_mouse();
}
