#ifdef _WIN32
#include "event_executor.hh"
#include "logger.hh"

#include <map>
#include <memory>

#include <winuser.h>

#include <SDL2/SDL_events.h>

static constexpr int kDelayMicros = 12000;

static const std::map<SDL_Scancode, int> SDLScancodeToVK = {
    {SDL_SCANCODE_RETURN, VK_RETURN},
    {SDL_SCANCODE_ESCAPE, VK_ESCAPE},
    {SDL_SCANCODE_BACKSPACE, VK_BACK},
    {SDL_SCANCODE_TAB, VK_TAB},
    {SDL_SCANCODE_CAPSLOCK, VK_CAPITAL},
    {SDL_SCANCODE_F1, VK_F1},
    {SDL_SCANCODE_F2, VK_F2},
    {SDL_SCANCODE_F3, VK_F3},
    {SDL_SCANCODE_F4, VK_F4},
    {SDL_SCANCODE_F5, VK_F5},
    {SDL_SCANCODE_F6, VK_F6},
    {SDL_SCANCODE_F7, VK_F7},
    {SDL_SCANCODE_F8, VK_F8},
    {SDL_SCANCODE_F9, VK_F9},
    {SDL_SCANCODE_F10, VK_F10},
    {SDL_SCANCODE_F11, VK_F11},
    {SDL_SCANCODE_F12, VK_F12},
    {SDL_SCANCODE_PRINTSCREEN, VK_SNAPSHOT},
    {SDL_SCANCODE_SCROLLLOCK, VK_SCROLL},
    {SDL_SCANCODE_PAUSE, VK_PAUSE},
    {SDL_SCANCODE_INSERT, VK_INSERT},
    {SDL_SCANCODE_HOME, VK_HOME},
    {SDL_SCANCODE_PAGEUP, VK_PRIOR},
    {SDL_SCANCODE_DELETE, VK_DELETE},
    {SDL_SCANCODE_END, VK_END},
    {SDL_SCANCODE_PAGEDOWN, VK_NEXT},
    {SDL_SCANCODE_RIGHT, VK_RIGHT},
    {SDL_SCANCODE_LEFT, VK_LEFT},
    {SDL_SCANCODE_DOWN, VK_DOWN},
    {SDL_SCANCODE_UP, VK_UP},
    {SDL_SCANCODE_NUMLOCKCLEAR, VK_NUMLOCK},
    {SDL_SCANCODE_KP_DIVIDE, VK_DIVIDE},
    {SDL_SCANCODE_KP_MULTIPLY, VK_MULTIPLY},
    {SDL_SCANCODE_KP_MINUS, VK_SUBTRACT},
    {SDL_SCANCODE_KP_PLUS, VK_ADD},
    {SDL_SCANCODE_KP_ENTER, VK_RETURN},
    {SDL_SCANCODE_KP_PERIOD, VK_OEM_PERIOD},
    {SDL_SCANCODE_KP_1, VK_NUMPAD1},
    {SDL_SCANCODE_KP_2, VK_NUMPAD2},
    {SDL_SCANCODE_KP_3, VK_NUMPAD3},
    {SDL_SCANCODE_KP_4, VK_NUMPAD4},
    {SDL_SCANCODE_KP_5, VK_NUMPAD5},
    {SDL_SCANCODE_KP_6, VK_NUMPAD6},
    {SDL_SCANCODE_KP_7, VK_NUMPAD7},
    {SDL_SCANCODE_KP_8, VK_NUMPAD8},
    {SDL_SCANCODE_KP_9, VK_NUMPAD9},
    {SDL_SCANCODE_KP_0, VK_NUMPAD0},
    {SDL_SCANCODE_APPLICATION, VK_APPS},
    {SDL_SCANCODE_KP_EQUALS, VK_OEM_PLUS},
    {SDL_SCANCODE_F13, VK_F13},
    {SDL_SCANCODE_F14, VK_F14},
    {SDL_SCANCODE_F15, VK_F15},
    {SDL_SCANCODE_F16, VK_F16},
    {SDL_SCANCODE_F17, VK_F17},
    {SDL_SCANCODE_F18, VK_F18},
    {SDL_SCANCODE_F19, VK_F19},
    {SDL_SCANCODE_F20, VK_F20},
    {SDL_SCANCODE_F21, VK_F21},
    {SDL_SCANCODE_F22, VK_F22},
    {SDL_SCANCODE_F23, VK_F23},
    {SDL_SCANCODE_F24, VK_F24},
    {SDL_SCANCODE_EXECUTE, VK_EXECUTE},
    {SDL_SCANCODE_HELP, VK_HELP},
    {SDL_SCANCODE_MENU, VK_MENU},
    {SDL_SCANCODE_SELECT, VK_SELECT},
    /* {SDL_SCANCODE_STOP, VK_}, */
    /* {SDL_SCANCODE_AGAIN, VK_}, */
    /* {SDL_SCANCODE_UNDO, XK_Undo}, */
    /* {SDL_SCANCODE_FIND, XK_Find}, */
    {SDL_SCANCODE_KP_COMMA, VK_OEM_COMMA},
    /* {SDL_SCANCODE_SYSREQ, XK_Sys_Req}, */
    {SDL_SCANCODE_LCTRL, VK_LCONTROL},
    {SDL_SCANCODE_LSHIFT, VK_LSHIFT},
    {SDL_SCANCODE_LALT, VK_LMENU},
    {SDL_SCANCODE_LGUI, VK_LWIN},
    {SDL_SCANCODE_RGUI, VK_RWIN},
    {SDL_SCANCODE_RCTRL, VK_RCONTROL},
    {SDL_SCANCODE_RSHIFT, VK_RSHIFT},
    {SDL_SCANCODE_RALT, VK_RMENU},
    {SDL_SCANCODE_MODE, VK_MODECHANGE},
    {SDL_SCANCODE_PERIOD, VK_OEM_PERIOD},
    {SDL_SCANCODE_COMMA, VK_OEM_COMMA},
    {SDL_SCANCODE_SLASH, VK_OEM_2},
    {SDL_SCANCODE_BACKSLASH, VK_OEM_5},
    {SDL_SCANCODE_MINUS, VK_OEM_MINUS},
    {SDL_SCANCODE_EQUALS, VK_OEM_NEC_EQUAL},
    {SDL_SCANCODE_SPACE, VK_SPACE},
    /* {SDL_SCANCODE_GRAVE, XK_grave}, */
    /* {SDL_SCANCODE_APOSTROPHE, XK_apostrophe}, */
    {SDL_SCANCODE_LEFTBRACKET, VK_OEM_4},
    {SDL_SCANCODE_RIGHTBRACKET, VK_OEM_6},
};

auto into_vk(const SDL_Scancode &scancode) -> WORD
{
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return 0x41 + (scancode - SDL_SCANCODE_A);
    }
    if (scancode == SDL_SCANCODE_0) {
        return 0x30;
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return 0x31 + (scancode - SDL_SCANCODE_1);
    }

    if (SDLScancodeToVK.contains(scancode)) {
        return SDLScancodeToVK.at(scancode);
    }

    return VK_NONCONVERT;
}

static auto translate(const SDL_MouseMotionEvent &e, int w, int h) -> INPUT
{
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi = {
        .dx = static_cast<LONG>(double(e.x) / double(w) * 65536),
        .dy = static_cast<LONG>(double(e.y) / double(h) * 65536),
        .mouseData = 0,
        .dwFlags =
            MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
        .time = 0,
        .dwExtraInfo = 100,
    };
    return input;
}

static auto translate(const SDL_MouseWheelEvent &e) -> INPUT
{
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi = {.dx = 0,
                .dy = 0,
                .mouseData = 0,
                .dwFlags = 0,
                .time = 0,
                .dwExtraInfo = 0};
    if (e.y) {
        input.mi.mouseData = e.y > 0 ? 120 : -120;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    }
    if (e.x) {
        input.mi.mouseData = e.x > 0 ? 120 : -120;
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    }
    return input;
}

static auto translate(const SDL_MouseButtonEvent &e) -> INPUT
{
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi = {.dx = 0,
                .dy = 0,
                .mouseData = 0,
                .dwFlags = 0,
                .time = 0,
                .dwExtraInfo = 0};
    switch (e.button) {
    case SDL_BUTTON_LEFT:
        if (e.type == SDL_MOUSEBUTTONDOWN)
            input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        else
            input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        break;
    case SDL_BUTTON_RIGHT:
        if (e.type == SDL_MOUSEBUTTONDOWN)
            input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        else
            input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
        break;
    case SDL_BUTTON_MIDDLE:
        if (e.type == SDL_MOUSEBUTTONDOWN)
            input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
        else
            input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
        break;
    case SDL_BUTTON_X1:
    case SDL_BUTTON_X2:
        input.mi.mouseData = e.button == SDL_BUTTON_X1 ? XBUTTON1 : XBUTTON2;
        if (e.type == SDL_MOUSEBUTTONDOWN)
            input.mi.dwFlags |= MOUSEEVENTF_XDOWN;
        else
            input.mi.dwFlags |= MOUSEEVENTF_XUP;
        break;
    }
    return input;
}

auto translate(const SDL_KeyboardEvent &e) -> INPUT
{
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = into_vk(e.keysym.scancode);
    input.ki.wScan = 0;
    input.ki.dwFlags = (e.type == SDL_KEYDOWN ? 0 : KEYEVENTF_KEYUP);
    input.ki.dwExtraInfo = 0;
    input.ki.time = 0;
    logger::debug("got key {}: {} to VK: {}",
                  e.type == SDL_KEYDOWN ? "down" : "up",
                  SDL_GetKeyName(e.keysym.sym), input.ki.wVk);

    return input;
}

// TODO: bypass UIPI?
auto EventExecutor::execute(EventExecutor::Event ev) -> bool
{
    SDL_Event e = ev.native_ev;
    INPUT input;
    int sent;
    switch (e.type) {
    case SDL_EventType::SDL_MOUSEMOTION:
        input = translate(e.motion, window_width_, window_height_);
        SendInput(1, &input, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_MOUSEBUTTONDOWN:
    case SDL_EventType::SDL_MOUSEBUTTONUP:
        input = translate(e.button);
        SendInput(1, &input, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_MOUSEWHEEL:
        input = translate(e.wheel);
        SendInput(1, &input, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_KEYDOWN:
        input = translate(e.key);
        SendInput(1, &input, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_KEYUP: // FIXME: correctly handle key inputs
        input = translate(e.key);
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(INPUT));
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        break;
    default:
        break;
    }
    return true;
}

#endif
