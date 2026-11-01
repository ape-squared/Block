#pragma once

#include <array>
#include <bitset>
#include <string>
#include <unordered_map>

#include <glm/vec2.hpp>

#include "core/event_bus.hpp"
#include "key_codes.hpp"
#include "mouse_button_codes.hpp"

#if !defined(_WIN32)
typedef struct xkb_context xkb_context;
typedef struct xkb_keymap xkb_keymap;
typedef struct xkb_state xkb_state;
typedef uint32_t xkb_keycode_t;
typedef uint32_t xkb_keysym_t;
#endif

namespace vkShade
{
    constexpr KeyCode DEFAULT_KEY_EFFECTS_TOGGLE = KeyCode::KEY_INSERT;
    constexpr KeyCode DEFAULT_KEY_GUI_TOGGLE = KeyCode::KEY_B;

    // 0 = passthrough, 1 = block when ImGui wants capture, 2 = block while overlay open
    enum class InputProcessing : int
    {
        Passthrough = 0,
        BlockWhenWanted = 1,
        BlockWhileOverlayOpen = 2,
    };

    class InputManager
    {
    public:
        virtual ~InputManager();

        void bind_action(const std::string& actionName, vkShade::KeyCode key,
                         bool requireShift = false, bool requireCtrl = false, bool requireAlt = false);

        bool is_action_pressed(const std::string& actionName) const;
        bool is_action_just_pressed(const std::string& actionName) const;
        bool is_action_just_released(const std::string& actionName) const;

        bool is_key_down(KeyCode key) const;
        bool is_key_just_pressed(KeyCode key) const;
        bool is_virtual_key_down(uint32_t vkey) const;
        bool is_virtual_key_just_pressed(uint32_t vkey) const;

        bool is_mouse_down(int button) const;
        bool is_mouse_just_pressed(int button) const;
        glm::vec2 mouse_position() const { return m_mousePos; }
        glm::vec2 mouse_delta() const { return m_mouseDelta; }
        float mouse_wheel_delta() const { return m_mouseWheelDelta; }

        InputProcessing input_processing() const { return m_inputProcessing; }
        bool blocking_mouse() const;
        bool blocking_keyboard() const;
        bool want_text_input() const { return m_wantTextInput; }

        void set_overlay_visible(bool visible) { m_overlayVisible = visible; }
        void set_imgui_capture(bool wantMouse, bool wantKeyboard, bool wantTextInput);

        virtual void process_events() = 0;

        void update();

    protected:
        InputManager();  // Prevent direct instantiation

#if !defined(_WIN32)
        xkb_context* m_xkbContext {nullptr};
        xkb_keymap*  m_xkbKeymap  {nullptr};
        xkb_state*   m_xkbState   {nullptr};

        void handle_keyboard_event(const xkb_keycode_t& keycode, bool pressed);
        KeyCode map_key(xkb_keysym_t keysym);
#endif

        void handle_key_code_event(KeyCode key, bool pressed);
        void handle_text_input(const std::string& text);
        void handle_mouse_button_event(MouseButton button, bool pressed);
        void handle_mouse_motion_event(float x, float y);
        void handle_mouse_wheel_event(float x, float y);

        void on_keybind_changed(const std::string& configKey, std::string enumString);

        std::unordered_map<KeyCode, bool> m_currentKeyStates;
        std::unordered_map<KeyCode, bool> m_previousKeyStates;

        glm::vec2 m_mousePos {0.0f, 0.0f};
        glm::vec2 m_mouseDelta {0.0f, 0.0f};
        float m_mouseWheelDelta {0.0f};
        std::array<bool, 5> m_mouseButtonsDown {};
        std::array<bool, 5> m_mouseButtonsPrev {};
        std::array<bool, 5> m_mouseButtonsPressed {};

        InputProcessing m_inputProcessing {InputProcessing::BlockWhenWanted};
        bool m_overlayVisible {false};
        bool m_wantMouse {false};
        bool m_wantKeyboard {false};
        bool m_wantTextInput {false};

    private:
        EventBus& m_eventBus;

        struct ActionBinding
        {
            KeyCode keyCode;
            bool requireShift = false;
            bool requireCtrl = false;
            bool requireAlt = false;
            bool pressed = false;
            bool justPressed = false;
            bool justReleased = false;
        };

        std::unordered_map<std::string, ActionBinding> m_actionBindings;

        void parse_and_bind_action(const std::string& actionName, const std::string& binding,
                                   KeyCode defaultKey, bool defaultShift);
    };
} // namespace vkShade
