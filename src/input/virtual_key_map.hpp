#pragma once

#include "key_codes.hpp"

namespace vkShade
{
    // ReShade keycode annotations use Windows virtual-key codes.
    inline KeyCode keycode_from_virtual_key(int vkey)
    {
        if (vkey >= '0' && vkey <= '9')
            return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KEY_0) + (vkey - '0'));
        if (vkey >= 'A' && vkey <= 'Z')
            return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::KEY_A) + (vkey - 'A'));

        switch (vkey)
        {
            case 0xC0: return KeyCode::KEY_BACKQUOTE;      // VK_OEM_3
            case 0x09: return KeyCode::KEY_TAB;            // VK_TAB
            case 0xA0: return KeyCode::KEY_LSHIFT;         // VK_LSHIFT
            case 0xA1: return KeyCode::KEY_RSHIFT;         // VK_RSHIFT
            case 0xA2: return KeyCode::KEY_LCTRL;          // VK_LCONTROL
            case 0xA3: return KeyCode::KEY_RCTRL;          // VK_RCONTROL
            case 0xA4: return KeyCode::KEY_LALT;           // VK_LMENU
            case 0xA5: return KeyCode::KEY_RALT;           // VK_RMENU
            case 0x20: return KeyCode::KEY_SPACE;          // VK_SPACE
            case 0x1B: return KeyCode::KEY_ESCAPE;         // VK_ESCAPE
            case 0x08: return KeyCode::KEY_BACKSPACE;      // VK_BACK
            case 0x0D: return KeyCode::KEY_RETURN;         // VK_RETURN
            case 0xBB: return KeyCode::KEY_EQUALS;         // VK_OEM_PLUS
            case 0xBD: return KeyCode::KEY_MINUS;          // VK_OEM_MINUS
            case 0xDB: return KeyCode::KEY_LEFTBRACKET;    // VK_OEM_4
            case 0xDD: return KeyCode::KEY_RIGHTBRACKET;   // VK_OEM_6
            case 0xDC: return KeyCode::KEY_BACKSLASH;      // VK_OEM_5
            case 0xBA: return KeyCode::KEY_SEMICOLON;      // VK_OEM_1
            case 0xDE: return KeyCode::KEY_QUOTE;          // VK_OEM_7
            case 0xBF: return KeyCode::KEY_SLASH;          // VK_OEM_2
            case 0xBC: return KeyCode::KEY_COMMA;          // VK_OEM_COMMA
            case 0xBE: return KeyCode::KEY_PERIOD;         // VK_OEM_PERIOD
            case 0x2D: return KeyCode::KEY_INSERT;         // VK_INSERT
            case 0x2E: return KeyCode::KEY_DELETE;         // VK_DELETE
            case 0x24: return KeyCode::KEY_HOME;           // VK_HOME
            case 0x23: return KeyCode::KEY_END;            // VK_END
            case 0x21: return KeyCode::KEY_PAGEUP;         // VK_PRIOR
            case 0x22: return KeyCode::KEY_PAGEDOWN;       // VK_NEXT
            case 0x2C: return KeyCode::KEY_PRINTSCREEN;    // VK_SNAPSHOT
            case 0x91: return KeyCode::KEY_SCROLLLOCK;     // VK_SCROLL
            case 0x13: return KeyCode::KEY_PAUSE;          // VK_PAUSE
            case 0x26: return KeyCode::KEY_UP;             // VK_UP
            case 0x28: return KeyCode::KEY_DOWN;           // VK_DOWN
            case 0x25: return KeyCode::KEY_LEFT;           // VK_LEFT
            case 0x27: return KeyCode::KEY_RIGHT;          // VK_RIGHT
            case 0x6B: return KeyCode::KEY_NUMPAD_PLUS;    // VK_ADD
            case 0x6D: return KeyCode::KEY_NUMPAD_MINUS;   // VK_SUBTRACT
            case 0x6A: return KeyCode::KEY_NUMPAD_MULTIPLY;// VK_MULTIPLY
            case 0x6F: return KeyCode::KEY_NUMPAD_DIVIDE;  // VK_DIVIDE
            case 0x6E: return KeyCode::KEY_NUMPAD_PERIOD;  // VK_DECIMAL
            case 0x60: return KeyCode::KEY_NUMPAD_0;
            case 0x61: return KeyCode::KEY_NUMPAD_1;
            case 0x62: return KeyCode::KEY_NUMPAD_2;
            case 0x63: return KeyCode::KEY_NUMPAD_3;
            case 0x64: return KeyCode::KEY_NUMPAD_4;
            case 0x65: return KeyCode::KEY_NUMPAD_5;
            case 0x66: return KeyCode::KEY_NUMPAD_6;
            case 0x67: return KeyCode::KEY_NUMPAD_7;
            case 0x68: return KeyCode::KEY_NUMPAD_8;
            case 0x69: return KeyCode::KEY_NUMPAD_9;
            case 0x70: return KeyCode::KEY_F1;
            case 0x71: return KeyCode::KEY_F2;
            case 0x72: return KeyCode::KEY_F3;
            case 0x73: return KeyCode::KEY_F4;
            case 0x74: return KeyCode::KEY_F5;
            case 0x75: return KeyCode::KEY_F6;
            case 0x76: return KeyCode::KEY_F7;
            case 0x77: return KeyCode::KEY_F8;
            case 0x78: return KeyCode::KEY_F9;
            case 0x79: return KeyCode::KEY_F10;
            case 0x7A: return KeyCode::KEY_F11;
            case 0x7B: return KeyCode::KEY_F12;
            case 0x10: return KeyCode::KEY_LSHIFT;         // VK_SHIFT
            case 0x11: return KeyCode::KEY_LCTRL;          // VK_CONTROL
            case 0x12: return KeyCode::KEY_LALT;           // VK_MENU
            default:   return KeyCode::KEY_UNKNOWN;
        }
    }

    inline int virtual_key_from_keycode(KeyCode code)
    {
        switch (code)
        {
            case KeyCode::KEY_0: return '0';
            case KeyCode::KEY_1: return '1';
            case KeyCode::KEY_2: return '2';
            case KeyCode::KEY_3: return '3';
            case KeyCode::KEY_4: return '4';
            case KeyCode::KEY_5: return '5';
            case KeyCode::KEY_6: return '6';
            case KeyCode::KEY_7: return '7';
            case KeyCode::KEY_8: return '8';
            case KeyCode::KEY_9: return '9';
            case KeyCode::KEY_A: return 'A';
            case KeyCode::KEY_B: return 'B';
            case KeyCode::KEY_C: return 'C';
            case KeyCode::KEY_D: return 'D';
            case KeyCode::KEY_E: return 'E';
            case KeyCode::KEY_F: return 'F';
            case KeyCode::KEY_G: return 'G';
            case KeyCode::KEY_H: return 'H';
            case KeyCode::KEY_I: return 'I';
            case KeyCode::KEY_J: return 'J';
            case KeyCode::KEY_K: return 'K';
            case KeyCode::KEY_L: return 'L';
            case KeyCode::KEY_M: return 'M';
            case KeyCode::KEY_N: return 'N';
            case KeyCode::KEY_O: return 'O';
            case KeyCode::KEY_P: return 'P';
            case KeyCode::KEY_Q: return 'Q';
            case KeyCode::KEY_R: return 'R';
            case KeyCode::KEY_S: return 'S';
            case KeyCode::KEY_T: return 'T';
            case KeyCode::KEY_U: return 'U';
            case KeyCode::KEY_V: return 'V';
            case KeyCode::KEY_W: return 'W';
            case KeyCode::KEY_X: return 'X';
            case KeyCode::KEY_Y: return 'Y';
            case KeyCode::KEY_Z: return 'Z';
            case KeyCode::KEY_BACKQUOTE: return 0xC0;
            case KeyCode::KEY_TAB: return 0x09;
            case KeyCode::KEY_LSHIFT: return 0xA0;
            case KeyCode::KEY_RSHIFT: return 0xA1;
            case KeyCode::KEY_LCTRL: return 0xA2;
            case KeyCode::KEY_RCTRL: return 0xA3;
            case KeyCode::KEY_LALT: return 0xA4;
            case KeyCode::KEY_RALT: return 0xA5;
            case KeyCode::KEY_SPACE: return 0x20;
            case KeyCode::KEY_ESCAPE: return 0x1B;
            case KeyCode::KEY_BACKSPACE: return 0x08;
            case KeyCode::KEY_RETURN: return 0x0D;
            case KeyCode::KEY_EQUALS: return 0xBB;
            case KeyCode::KEY_MINUS: return 0xBD;
            case KeyCode::KEY_LEFTBRACKET: return 0xDB;
            case KeyCode::KEY_RIGHTBRACKET: return 0xDD;
            case KeyCode::KEY_BACKSLASH: return 0xDC;
            case KeyCode::KEY_SEMICOLON: return 0xBA;
            case KeyCode::KEY_QUOTE: return 0xDE;
            case KeyCode::KEY_SLASH: return 0xBF;
            case KeyCode::KEY_COMMA: return 0xBC;
            case KeyCode::KEY_PERIOD: return 0xBE;
            case KeyCode::KEY_INSERT: return 0x2D;
            case KeyCode::KEY_DELETE: return 0x2E;
            case KeyCode::KEY_HOME: return 0x24;
            case KeyCode::KEY_END: return 0x23;
            case KeyCode::KEY_PAGEUP: return 0x21;
            case KeyCode::KEY_PAGEDOWN: return 0x22;
            case KeyCode::KEY_PRINTSCREEN: return 0x2C;
            case KeyCode::KEY_SCROLLLOCK: return 0x91;
            case KeyCode::KEY_PAUSE: return 0x13;
            case KeyCode::KEY_UP: return 0x26;
            case KeyCode::KEY_DOWN: return 0x28;
            case KeyCode::KEY_LEFT: return 0x25;
            case KeyCode::KEY_RIGHT: return 0x27;
            case KeyCode::KEY_NUMPAD_PLUS: return 0x6B;
            case KeyCode::KEY_NUMPAD_MINUS: return 0x6D;
            case KeyCode::KEY_NUMPAD_MULTIPLY: return 0x6A;
            case KeyCode::KEY_NUMPAD_DIVIDE: return 0x6F;
            case KeyCode::KEY_NUMPAD_PERIOD: return 0x6E;
            case KeyCode::KEY_NUMPAD_0: return 0x60;
            case KeyCode::KEY_NUMPAD_1: return 0x61;
            case KeyCode::KEY_NUMPAD_2: return 0x62;
            case KeyCode::KEY_NUMPAD_3: return 0x63;
            case KeyCode::KEY_NUMPAD_4: return 0x64;
            case KeyCode::KEY_NUMPAD_5: return 0x65;
            case KeyCode::KEY_NUMPAD_6: return 0x66;
            case KeyCode::KEY_NUMPAD_7: return 0x67;
            case KeyCode::KEY_NUMPAD_8: return 0x68;
            case KeyCode::KEY_NUMPAD_9: return 0x69;
            case KeyCode::KEY_F1: return 0x70;
            case KeyCode::KEY_F2: return 0x71;
            case KeyCode::KEY_F3: return 0x72;
            case KeyCode::KEY_F4: return 0x73;
            case KeyCode::KEY_F5: return 0x74;
            case KeyCode::KEY_F6: return 0x75;
            case KeyCode::KEY_F7: return 0x76;
            case KeyCode::KEY_F8: return 0x77;
            case KeyCode::KEY_F9: return 0x78;
            case KeyCode::KEY_F10: return 0x79;
            case KeyCode::KEY_F11: return 0x7A;
            case KeyCode::KEY_F12: return 0x7B;
            default: return 0;
        }
    }
} // namespace vkShade
