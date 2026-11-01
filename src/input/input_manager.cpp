#include "input_manager.hpp"
#include "input/key_codes.hpp"
#include "input/virtual_key_map.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <magic_enum/magic_enum.hpp>
#include "core/logger.hpp"

#if !defined(_WIN32)
#include <xkbcommon/xkbcommon.h>
#endif

#include "config/config_manager.hpp"
#include "core/service_locator.hpp"
#include "input_events.hpp"

vkShade::InputManager::InputManager()
    : m_eventBus(vkShade::Locator<EventBus>::get())
{
    Logger::debug("Initializing InputManager");

#if !defined(_WIN32)
    m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_xkbContext)
    {
        Logger::error("[InputManager] Failed to create XKB context");
        return;
    }

    struct xkb_rule_names names = {
        .rules = nullptr,
        .model = nullptr,
        .layout = nullptr,
        .variant = nullptr,
        .options = nullptr
    };

    m_xkbKeymap = xkb_keymap_new_from_names(m_xkbContext, &names,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!m_xkbKeymap)
    {
        Logger::error("[InputManager] Failed to create XKB keymap");
        return;
    }

    m_xkbState = xkb_state_new(m_xkbKeymap);
    if (!m_xkbState)
    {
        Logger::error("[InputManager] Failed to create XKB state");
        return;
    }
#endif

    auto& config = vkShade::Locator<vkShade::ConfigManager>::get().app();

    if (auto processing = config.get<int32_t>("Input", "InputProcessing"))
        m_inputProcessing = static_cast<InputProcessing>(std::clamp(*processing, 0, 2));

    {
        auto toggleEffects = config.get<std::string>("Input", "ToggleEffects");
        parse_and_bind_action("ToggleEffects", toggleEffects.value_or(""),
                              DEFAULT_KEY_EFFECTS_TOGGLE, false);
        config.on_changed("Input", "ToggleEffects").connect<&InputManager::on_keybind_changed>(this);
    }

    {
        auto toggleGui = config.get<std::string>("Input", "ToggleGui");
        parse_and_bind_action("ToggleGui", toggleGui.value_or(""),
                              DEFAULT_KEY_GUI_TOGGLE, true);
        config.on_changed("Input", "ToggleGui").connect<&InputManager::on_keybind_changed>(this);
    }
}

vkShade::InputManager::~InputManager()
{
#if !defined(_WIN32)
    if (m_xkbState)
    {
        xkb_state_unref(m_xkbState);
        m_xkbState = nullptr;
    }
    if (m_xkbKeymap)
    {
        xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap = nullptr;
    }
    if (m_xkbContext)
    {
        xkb_context_unref(m_xkbContext);
        m_xkbContext = nullptr;
    }
#endif
}

void vkShade::InputManager::parse_and_bind_action(const std::string& actionName,
                                                  const std::string& binding,
                                                  KeyCode defaultKey,
                                                  bool defaultShift)
{
    bool requireShift = defaultShift;
    bool requireCtrl = false;
    bool requireAlt = false;
    KeyCode key = defaultKey;

    if (!binding.empty())
    {
        std::stringstream ss(binding);
        std::string token;
        while (std::getline(ss, token, '+'))
        {
            // trim
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())))
                token.erase(token.begin());
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))
                token.pop_back();

            std::string upper = token;
            for (auto& c : upper)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            if (upper == "SHIFT")
                requireShift = true;
            else if (upper == "CTRL" || upper == "CONTROL")
                requireCtrl = true;
            else if (upper == "ALT")
                requireAlt = true;
            else
            {
                auto keyEnum = magic_enum::enum_cast<KeyCode>(token);
                if (!keyEnum && token.rfind("KEY_", 0) != 0)
                    keyEnum = magic_enum::enum_cast<KeyCode>("KEY_" + upper);
                if (keyEnum)
                    key = *keyEnum;
            }
        }
    }

    bind_action(actionName, key, requireShift, requireCtrl, requireAlt);
}

void vkShade::InputManager::bind_action(const std::string& actionName, vkShade::KeyCode keyCode,
                                        bool requireShift, bool requireCtrl, bool requireAlt)
{
    m_actionBindings[actionName] = ActionBinding {keyCode, requireShift, requireCtrl, requireAlt};

    std::string label;
    if (requireCtrl) label += "Ctrl+";
    if (requireAlt) label += "Alt+";
    if (requireShift) label += "Shift+";
    label += magic_enum::enum_name(keyCode);
    Logger::debug("Bound action '{}' to '{}'", actionName, label);
}

void vkShade::InputManager::handle_key_code_event(KeyCode key, bool pressed)
{
    m_currentKeyStates[key] = pressed;
    m_eventBus.enqueue(KeyboardEvent{key, pressed});
}

void vkShade::InputManager::handle_text_input(const std::string& text)
{
    if (!text.empty())
        m_eventBus.enqueue(TextInputEvent{.text = text});
}

#if !defined(_WIN32)
void vkShade::InputManager::handle_keyboard_event(const xkb_keycode_t& keycode, bool pressed)
{
    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(m_xkbState, keycode);

    handle_key_code_event(map_key(keysym), pressed);

    if (!pressed)
        return;

    char buffer[64];
    const int length = xkb_state_key_get_utf8(
        m_xkbState, keycode, buffer, sizeof(buffer));

    if (length > 0)
        handle_text_input(std::string(buffer, length));
}
#endif

void vkShade::InputManager::handle_mouse_button_event(MouseButton button, bool pressed)
{
    const auto index = static_cast<size_t>(button);
    if (index < m_mouseButtonsDown.size())
        m_mouseButtonsDown[index] = pressed;
    m_eventBus.enqueue(MouseButtonEvent{button, pressed});
}

void vkShade::InputManager::handle_mouse_motion_event(float x, float y)
{
    const glm::vec2 pos {x, y};
    m_mouseDelta += pos - m_mousePos;
    m_mousePos = pos;
    m_eventBus.enqueue(MouseMotionEvent{x, y});
}

void vkShade::InputManager::handle_mouse_wheel_event(float x, float y)
{
    m_mouseWheelDelta += y;
    m_eventBus.enqueue(MouseWheelEvent{x, y});
}

bool vkShade::InputManager::is_action_pressed(const std::string& actionName) const
{
    auto it = m_actionBindings.find(actionName);
    return it != m_actionBindings.end() ? it->second.pressed : false;
}

bool vkShade::InputManager::is_action_just_pressed(const std::string& actionName) const
{
    auto it = m_actionBindings.find(actionName);
    return it != m_actionBindings.end() ? it->second.justPressed : false;
}

bool vkShade::InputManager::is_action_just_released(const std::string& actionName) const
{
    auto it = m_actionBindings.find(actionName);
    return it != m_actionBindings.end() ? it->second.justReleased : false;
}

bool vkShade::InputManager::is_key_down(KeyCode key) const
{
    auto it = m_currentKeyStates.find(key);
    return it != m_currentKeyStates.end() && it->second;
}

bool vkShade::InputManager::is_key_just_pressed(KeyCode key) const
{
    const bool current = is_key_down(key);
    auto prev = m_previousKeyStates.find(key);
    const bool previous = prev != m_previousKeyStates.end() && prev->second;
    return current && !previous;
}

bool vkShade::InputManager::is_virtual_key_down(uint32_t vkey) const
{
    if (vkey >= 256)
        return false;
    return is_key_down(keycode_from_virtual_key(static_cast<int>(vkey)));
}

bool vkShade::InputManager::is_virtual_key_just_pressed(uint32_t vkey) const
{
    if (vkey >= 256)
        return false;
    return is_key_just_pressed(keycode_from_virtual_key(static_cast<int>(vkey)));
}

bool vkShade::InputManager::is_mouse_down(int button) const
{
    if (button < 0 || button >= static_cast<int>(m_mouseButtonsDown.size()))
        return false;
    return m_mouseButtonsDown[static_cast<size_t>(button)];
}

bool vkShade::InputManager::is_mouse_just_pressed(int button) const
{
    if (button < 0 || button >= static_cast<int>(m_mouseButtonsPressed.size()))
        return false;
    return m_mouseButtonsPressed[static_cast<size_t>(button)];
}

bool vkShade::InputManager::blocking_mouse() const
{
    switch (m_inputProcessing)
    {
        case InputProcessing::Passthrough:
            return false;
        case InputProcessing::BlockWhileOverlayOpen:
            return m_overlayVisible;
        case InputProcessing::BlockWhenWanted:
        default:
            return m_overlayVisible && m_wantMouse;
    }
}

bool vkShade::InputManager::blocking_keyboard() const
{
    switch (m_inputProcessing)
    {
        case InputProcessing::Passthrough:
            return false;
        case InputProcessing::BlockWhileOverlayOpen:
            return m_overlayVisible;
        case InputProcessing::BlockWhenWanted:
        default:
            return m_overlayVisible && m_wantKeyboard;
    }
}

void vkShade::InputManager::set_imgui_capture(bool wantMouse, bool wantKeyboard, bool wantTextInput)
{
    m_wantMouse = wantMouse;
    m_wantKeyboard = wantKeyboard;
    m_wantTextInput = wantTextInput;
}

#if !defined(_WIN32)
vkShade::KeyCode vkShade::InputManager::map_key(xkb_keysym_t keysym)
{
    switch (keysym)
    {
        case XKB_KEY_0:            return KeyCode::KEY_0;
        case XKB_KEY_1:            return KeyCode::KEY_1;
        case XKB_KEY_2:            return KeyCode::KEY_2;
        case XKB_KEY_3:            return KeyCode::KEY_3;
        case XKB_KEY_4:            return KeyCode::KEY_4;
        case XKB_KEY_5:            return KeyCode::KEY_5;
        case XKB_KEY_6:            return KeyCode::KEY_6;
        case XKB_KEY_7:            return KeyCode::KEY_7;
        case XKB_KEY_8:            return KeyCode::KEY_8;
        case XKB_KEY_9:            return KeyCode::KEY_9;
        case XKB_KEY_a:            return KeyCode::KEY_A;
        case XKB_KEY_b:            return KeyCode::KEY_B;
        case XKB_KEY_c:            return KeyCode::KEY_C;
        case XKB_KEY_d:            return KeyCode::KEY_D;
        case XKB_KEY_e:            return KeyCode::KEY_E;
        case XKB_KEY_f:            return KeyCode::KEY_F;
        case XKB_KEY_g:            return KeyCode::KEY_G;
        case XKB_KEY_h:            return KeyCode::KEY_H;
        case XKB_KEY_i:            return KeyCode::KEY_I;
        case XKB_KEY_j:            return KeyCode::KEY_J;
        case XKB_KEY_k:            return KeyCode::KEY_K;
        case XKB_KEY_l:            return KeyCode::KEY_L;
        case XKB_KEY_m:            return KeyCode::KEY_M;
        case XKB_KEY_n:            return KeyCode::KEY_N;
        case XKB_KEY_o:            return KeyCode::KEY_O;
        case XKB_KEY_p:            return KeyCode::KEY_P;
        case XKB_KEY_q:            return KeyCode::KEY_Q;
        case XKB_KEY_r:            return KeyCode::KEY_R;
        case XKB_KEY_s:            return KeyCode::KEY_S;
        case XKB_KEY_t:            return KeyCode::KEY_T;
        case XKB_KEY_u:            return KeyCode::KEY_U;
        case XKB_KEY_v:            return KeyCode::KEY_V;
        case XKB_KEY_w:            return KeyCode::KEY_W;
        case XKB_KEY_x:            return KeyCode::KEY_X;
        case XKB_KEY_y:            return KeyCode::KEY_Y;
        case XKB_KEY_z:            return KeyCode::KEY_Z;
        case XKB_KEY_grave:        return KeyCode::KEY_BACKQUOTE;
        case XKB_KEY_Tab:          return KeyCode::KEY_TAB;
        case XKB_KEY_Shift_L:      return KeyCode::KEY_LSHIFT;
        case XKB_KEY_Shift_R:      return KeyCode::KEY_RSHIFT;
        case XKB_KEY_Control_L:    return KeyCode::KEY_LCTRL;
        case XKB_KEY_Control_R:    return KeyCode::KEY_RCTRL;
        case XKB_KEY_Alt_L:        return KeyCode::KEY_LALT;
        case XKB_KEY_Alt_R:        return KeyCode::KEY_RALT;
        case XKB_KEY_space:        return KeyCode::KEY_SPACE;
        case XKB_KEY_Escape:       return KeyCode::KEY_ESCAPE;
        case XKB_KEY_BackSpace:    return KeyCode::KEY_BACKSPACE;
        case XKB_KEY_Return:       return KeyCode::KEY_RETURN;
        case XKB_KEY_equal:        return KeyCode::KEY_EQUALS;
        case XKB_KEY_minus:        return KeyCode::KEY_MINUS;
        case XKB_KEY_bracketleft:  return KeyCode::KEY_LEFTBRACKET;
        case XKB_KEY_bracketright: return KeyCode::KEY_RIGHTBRACKET;
        case XKB_KEY_backslash:    return KeyCode::KEY_BACKSLASH;
        case XKB_KEY_semicolon:    return KeyCode::KEY_SEMICOLON;
        case XKB_KEY_apostrophe:   return KeyCode::KEY_QUOTE;
        case XKB_KEY_slash:        return KeyCode::KEY_SLASH;
        case XKB_KEY_comma:        return KeyCode::KEY_COMMA;
        case XKB_KEY_period:       return KeyCode::KEY_PERIOD;
        case XKB_KEY_Insert:       return KeyCode::KEY_INSERT;
        case XKB_KEY_Delete:       return KeyCode::KEY_DELETE;
        case XKB_KEY_Home:         return KeyCode::KEY_HOME;
        case XKB_KEY_End:          return KeyCode::KEY_END;
        case XKB_KEY_Page_Up:      return KeyCode::KEY_PAGEUP;
        case XKB_KEY_Page_Down:    return KeyCode::KEY_PAGEDOWN;
        case XKB_KEY_Print:        return KeyCode::KEY_PRINTSCREEN;
        case XKB_KEY_Scroll_Lock:  return KeyCode::KEY_SCROLLLOCK;
        case XKB_KEY_Pause:        return KeyCode::KEY_PAUSE;
        case XKB_KEY_Up:           return KeyCode::KEY_UP;
        case XKB_KEY_Down:         return KeyCode::KEY_DOWN;
        case XKB_KEY_Left:         return KeyCode::KEY_LEFT;
        case XKB_KEY_Right:        return KeyCode::KEY_RIGHT;
        case XKB_KEY_KP_Add:       return KeyCode::KEY_NUMPAD_PLUS;
        case XKB_KEY_KP_Subtract:  return KeyCode::KEY_NUMPAD_MINUS;
        case XKB_KEY_KP_Multiply:  return KeyCode::KEY_NUMPAD_MULTIPLY;
        case XKB_KEY_KP_Divide:    return KeyCode::KEY_NUMPAD_DIVIDE;
        case XKB_KEY_KP_Decimal:   return KeyCode::KEY_NUMPAD_PERIOD;
        case XKB_KEY_KP_Enter:     return KeyCode::KEY_NUMPAD_ENTER;
        case XKB_KEY_KP_0:         return KeyCode::KEY_NUMPAD_0;
        case XKB_KEY_KP_1:         return KeyCode::KEY_NUMPAD_1;
        case XKB_KEY_KP_2:         return KeyCode::KEY_NUMPAD_2;
        case XKB_KEY_KP_3:         return KeyCode::KEY_NUMPAD_3;
        case XKB_KEY_KP_4:         return KeyCode::KEY_NUMPAD_4;
        case XKB_KEY_KP_5:         return KeyCode::KEY_NUMPAD_5;
        case XKB_KEY_KP_6:         return KeyCode::KEY_NUMPAD_6;
        case XKB_KEY_KP_7:         return KeyCode::KEY_NUMPAD_7;
        case XKB_KEY_KP_8:         return KeyCode::KEY_NUMPAD_8;
        case XKB_KEY_KP_9:         return KeyCode::KEY_NUMPAD_9;
        case XKB_KEY_F1:           return KeyCode::KEY_F1;
        case XKB_KEY_F2:           return KeyCode::KEY_F2;
        case XKB_KEY_F3:           return KeyCode::KEY_F3;
        case XKB_KEY_F4:           return KeyCode::KEY_F4;
        case XKB_KEY_F5:           return KeyCode::KEY_F5;
        case XKB_KEY_F6:           return KeyCode::KEY_F6;
        case XKB_KEY_F7:           return KeyCode::KEY_F7;
        case XKB_KEY_F8:           return KeyCode::KEY_F8;
        case XKB_KEY_F9:           return KeyCode::KEY_F9;
        case XKB_KEY_F10:          return KeyCode::KEY_F10;
        case XKB_KEY_F11:          return KeyCode::KEY_F11;
        case XKB_KEY_F12:          return KeyCode::KEY_F12;
        default:                   return KeyCode::KEY_UNKNOWN;
    }
}
#endif

void vkShade::InputManager::on_keybind_changed(const std::string& configKey, std::string enumString)
{
    if (configKey == "ToggleEffects")
    {
        parse_and_bind_action(configKey, enumString, DEFAULT_KEY_EFFECTS_TOGGLE, false);
        return;
    }
    if (configKey == "ToggleGui")
    {
        parse_and_bind_action(configKey, enumString, DEFAULT_KEY_GUI_TOGGLE, true);
        return;
    }

    parse_and_bind_action(configKey, enumString, KeyCode::KEY_UNKNOWN, false);
}

void vkShade::InputManager::update()
{
    m_mouseDelta = {0.0f, 0.0f};
    m_mouseWheelDelta = 0.0f;

    this->process_events();

    for (size_t i = 0; i < m_mouseButtonsDown.size(); ++i)
    {
        m_mouseButtonsPressed[i] = m_mouseButtonsDown[i] && !m_mouseButtonsPrev[i];
        m_mouseButtonsPrev[i] = m_mouseButtonsDown[i];
    }

    const bool shiftDown =
        is_key_down(KeyCode::KEY_LSHIFT) || is_key_down(KeyCode::KEY_RSHIFT);
    const bool ctrlDown =
        is_key_down(KeyCode::KEY_LCTRL) || is_key_down(KeyCode::KEY_RCTRL);
    const bool altDown =
        is_key_down(KeyCode::KEY_LALT) || is_key_down(KeyCode::KEY_RALT);

    for (auto& [name, binding] : m_actionBindings)
    {
        bool keyDown = is_key_down(binding.keyCode);
        if (binding.requireShift)
            keyDown = keyDown && shiftDown;
        if (binding.requireCtrl)
            keyDown = keyDown && ctrlDown;
        if (binding.requireAlt)
            keyDown = keyDown && altDown;

        // Suppress hotkeys while typing into the overlay
        if (m_wantTextInput && name != "ToggleGui")
            keyDown = false;

        bool previous = binding.pressed;
        binding.justPressed = keyDown && !previous;
        binding.justReleased = !keyDown && previous;
        binding.pressed = keyDown;
    }

    m_previousKeyStates = m_currentKeyStates;
}
