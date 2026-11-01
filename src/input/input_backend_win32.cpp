#include "input_backend_win32.hpp"

#include "core/logger.hpp"
#include "input/virtual_key_map.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace
{
    constexpr UINT_PTR kSubclassId = 0xB10C;
}

vkShade::InputBackendWin32::InputBackendWin32(HWND hwnd)
    : m_hwnd(hwnd)
{
    Logger::debug("Initializing Win32 input backend");

    if (m_hwnd && IsWindow(m_hwnd))
    {
        if (SetWindowSubclass(m_hwnd, &InputBackendWin32::subclass_proc, kSubclassId,
                              reinterpret_cast<DWORD_PTR>(this)))
        {
            m_subclassed = true;
        }
        else
        {
            Logger::warn("Failed to subclass HWND for input capture");
        }
    }
}

vkShade::InputBackendWin32::~InputBackendWin32()
{
    if (m_subclassed && m_hwnd && IsWindow(m_hwnd))
        RemoveWindowSubclass(m_hwnd, &InputBackendWin32::subclass_proc, kSubclassId);
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

LRESULT CALLBACK vkShade::InputBackendWin32::subclass_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                           UINT_PTR, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<InputBackendWin32*>(refData);
    if (!self)
        return DefSubclassProc(hwnd, msg, wParam, lParam);

    const bool blockMouse = self->blocking_mouse();
    const bool blockKeyboard = self->blocking_keyboard();

    switch (msg)
    {
        case WM_MOUSEWHEEL:
        {
            const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
            self->handle_mouse_wheel_event(0.0f, delta);
            if (blockMouse)
                return 0;
            break;
        }
        case WM_MOUSEHWHEEL:
        {
            const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
            self->handle_mouse_wheel_event(delta, 0.0f);
            if (blockMouse)
                return 0;
            break;
        }
        case WM_CHAR:
        {
            if (wParam >= 32)
            {
                char utf8[8] {};
                const wchar_t ch = static_cast<wchar_t>(wParam);
                const int len = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, utf8, sizeof(utf8), nullptr, nullptr);
                if (len > 0)
                    self->handle_text_input(std::string(utf8, len));
            }
            if (blockKeyboard)
                return 0;
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
            if (blockMouse)
                return 0;
            break;
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            if (blockKeyboard)
                return 0;
            break;
        default:
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
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
        const KeyCode code = keycode_from_virtual_key(vkey);
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
    const bool x1 = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
    const bool x2 = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;

    if (left != m_prevLeftButton)
        handle_mouse_button_event(MouseButton::LEFT, left);
    if (right != m_prevRightButton)
        handle_mouse_button_event(MouseButton::RIGHT, right);
    if (middle != m_prevMiddleButton)
        handle_mouse_button_event(MouseButton::MIDDLE, middle);
    if (x1 != m_prevX1Button)
        handle_mouse_button_event(MouseButton::X1, x1);
    if (x2 != m_prevX2Button)
        handle_mouse_button_event(MouseButton::X2, x2);

    m_prevLeftButton = left;
    m_prevMiddleButton = middle;
    m_prevRightButton = right;
    m_prevX1Button = x1;
    m_prevX2Button = x2;
}

void vkShade::InputBackendWin32::process_events()
{
    poll_keyboard();
    poll_mouse();
}
