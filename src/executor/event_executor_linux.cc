#ifdef __linux__
#include "event_executor.hh"
#include "logger.hh"

#include <map>
#include <memory>

#include <SDL2/SDL_events.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xdo.h>

static constexpr int kDelayMicros = 12000;
static const std::map<SDL_Scancode, int> SDLScancodeToXK = {
    {SDL_SCANCODE_AGAIN, XK_Redo},
    {SDL_SCANCODE_APOSTROPHE, XK_apostrophe},
    {SDL_SCANCODE_APPLICATION, XK_Hyper_R},
    {SDL_SCANCODE_BACKSLASH, XK_backslash},
    {SDL_SCANCODE_BACKSPACE, XK_BackSpace},
    {SDL_SCANCODE_CAPSLOCK, XK_Caps_Lock},
    {SDL_SCANCODE_COMMA, XK_comma},
    {SDL_SCANCODE_DELETE, XK_Delete},
    {SDL_SCANCODE_DOWN, XK_Down},
    {SDL_SCANCODE_END, XK_End},
    {SDL_SCANCODE_EQUALS, XK_equal},
    {SDL_SCANCODE_ESCAPE, XK_Escape},
    {SDL_SCANCODE_EXECUTE, XK_Execute},
    {SDL_SCANCODE_F1, XK_F1},
    {SDL_SCANCODE_F2, XK_F2},
    {SDL_SCANCODE_F3, XK_F3},
    {SDL_SCANCODE_F4, XK_F4},
    {SDL_SCANCODE_F5, XK_F5},
    {SDL_SCANCODE_F6, XK_F6},
    {SDL_SCANCODE_F7, XK_F7},
    {SDL_SCANCODE_F8, XK_F8},
    {SDL_SCANCODE_F9, XK_F9},
    {SDL_SCANCODE_F10, XK_F10},
    {SDL_SCANCODE_F11, XK_F11},
    {SDL_SCANCODE_F12, XK_F12},
    {SDL_SCANCODE_F13, XK_F13},
    {SDL_SCANCODE_F14, XK_F14},
    {SDL_SCANCODE_F15, XK_F15},
    {SDL_SCANCODE_F16, XK_F16},
    {SDL_SCANCODE_F17, XK_F17},
    {SDL_SCANCODE_F18, XK_F18},
    {SDL_SCANCODE_F19, XK_F19},
    {SDL_SCANCODE_F20, XK_F20},
    {SDL_SCANCODE_F21, XK_F21},
    {SDL_SCANCODE_F22, XK_F22},
    {SDL_SCANCODE_F23, XK_F23},
    {SDL_SCANCODE_F24, XK_F24},
    {SDL_SCANCODE_FIND, XK_Find},
    {SDL_SCANCODE_GRAVE, XK_grave},
    {SDL_SCANCODE_HELP, XK_Help},
    {SDL_SCANCODE_HOME, XK_Home},
    {SDL_SCANCODE_INSERT, XK_Insert},
    {SDL_SCANCODE_KP_0, XK_KP_0},
    {SDL_SCANCODE_KP_0, XK_KP_Insert},
    {SDL_SCANCODE_KP_1, XK_KP_1},
    {SDL_SCANCODE_KP_1, XK_KP_End},
    {SDL_SCANCODE_KP_2, XK_KP_2},
    {SDL_SCANCODE_KP_2, XK_KP_Down},
    {SDL_SCANCODE_KP_3, XK_KP_3},
    {SDL_SCANCODE_KP_3, XK_KP_Next},
    {SDL_SCANCODE_KP_4, XK_KP_4},
    {SDL_SCANCODE_KP_4, XK_KP_Left},
    {SDL_SCANCODE_KP_5, XK_KP_5},
    {SDL_SCANCODE_KP_5, XK_KP_Begin},
    {SDL_SCANCODE_KP_6, XK_KP_6},
    {SDL_SCANCODE_KP_6, XK_KP_Right},
    {SDL_SCANCODE_KP_7, XK_KP_7},
    {SDL_SCANCODE_KP_7, XK_KP_Home},
    {SDL_SCANCODE_KP_8, XK_KP_8},
    {SDL_SCANCODE_KP_8, XK_KP_Up},
    {SDL_SCANCODE_KP_9, XK_KP_9},
    {SDL_SCANCODE_KP_9, XK_KP_Prior},
    {SDL_SCANCODE_KP_COMMA, XK_KP_Separator},
    {SDL_SCANCODE_KP_DIVIDE, XK_KP_Divide},
    {SDL_SCANCODE_KP_ENTER, XK_KP_Enter},
    {SDL_SCANCODE_KP_EQUALS, XK_KP_Equal},
    {SDL_SCANCODE_KP_MINUS, XK_KP_Subtract},
    {SDL_SCANCODE_KP_MULTIPLY, XK_KP_Multiply},
    {SDL_SCANCODE_KP_PERIOD, XK_KP_Delete},
    {SDL_SCANCODE_KP_PLUS, XK_KP_Add},
    {SDL_SCANCODE_LALT, XK_Alt_L},
    {SDL_SCANCODE_LCTRL, XK_Control_L},
    {SDL_SCANCODE_LEFT, XK_Left},
    {SDL_SCANCODE_LEFTBRACKET, XK_bracketleft},
    {SDL_SCANCODE_LGUI, XK_Meta_L},
    {SDL_SCANCODE_LGUI, XK_Super_L},
    {SDL_SCANCODE_LSHIFT, XK_Shift_L},
    {SDL_SCANCODE_RALT, XK_Alt_R},
    {SDL_SCANCODE_RCTRL, XK_Control_R},
    {SDL_SCANCODE_RETURN, XK_Return},
    {SDL_SCANCODE_RGUI, XK_Meta_R},
    {SDL_SCANCODE_RSHIFT, XK_Shift_R},
    {SDL_SCANCODE_MENU, XK_Menu},
    {SDL_SCANCODE_MINUS, XK_minus},
    {SDL_SCANCODE_MODE, XK_Mode_switch},
    {SDL_SCANCODE_NUMLOCKCLEAR, XK_Num_Lock},
    {SDL_SCANCODE_PAGEDOWN, XK_Next},
    {SDL_SCANCODE_PAGEUP, XK_Prior},
    {SDL_SCANCODE_PAUSE, XK_Pause},
    {SDL_SCANCODE_PERIOD, XK_period},
    {SDL_SCANCODE_PRINTSCREEN, XK_Print},
    {SDL_SCANCODE_RIGHT, XK_Right},
    {SDL_SCANCODE_RIGHTBRACKET, XK_bracketright},
    {SDL_SCANCODE_SCROLLLOCK, XK_Scroll_Lock},
    {SDL_SCANCODE_SELECT, XK_Select},
    {SDL_SCANCODE_SLASH, XK_slash},
    {SDL_SCANCODE_SPACE, XK_space},
    {SDL_SCANCODE_STOP, XK_Cancel},
    {SDL_SCANCODE_SYSREQ, XK_Sys_Req},
    {SDL_SCANCODE_TAB, XK_Tab},
    {SDL_SCANCODE_UNDO, XK_Undo},
    {SDL_SCANCODE_UP, XK_Up},
};

static KeySym SDLScancodeToX11Keysym(SDL_Scancode scancode)
{
    KeySym keysym;
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return XK_a + (scancode - SDL_SCANCODE_A);
    }
    if (scancode == SDL_SCANCODE_0) {
        return XK_0;
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return XK_1 + (scancode - SDL_SCANCODE_1);
    }

    if (SDLScancodeToXK.contains(scancode)) {
        return SDLScancodeToXK.at(scancode);
    }
    return NoSymbol;
}

static auto translate(SDL_MouseMotionEvent &e, int ww, int wh, int rw, int rh)
    -> EventExecutor::Pos
{
    return EventExecutor::Pos{
        static_cast<int>(double(e.x) / ww * rw),
        static_cast<int>(double(e.y) / wh * rh),
    };
}

static auto translate(const SDL_KeyboardEvent &e) -> std::string
{
    std::string seq;
    const char *xkey =
        XKeysymToString(SDLScancodeToX11Keysym(e.keysym.scancode));
    seq += xkey ? xkey : "";
    return seq;
}

auto EventExecutor::execute(EventExecutor::Event ev) -> bool
{
    SDL_Event e = ev.native_ev;
    int x, y, screen_num;
    static Pos pos = {0, 0};
    std::string seq;
    xdo_get_mouse_location(xdo_, &x, &y, &screen_num);
    switch (e.type) {
    case SDL_EventType::SDL_MOUSEMOTION:
        pos = translate(e.motion, window_width_, window_height_, remote_width_,
                        remote_height_);
        xdo_move_mouse(xdo_, pos.x, pos.y, screen_num);
        break;
    case SDL_EventType::SDL_MOUSEBUTTONDOWN:
        xdo_mouse_down(xdo_, CURRENTWINDOW, e.button.button);
        break;
    case SDL_EventType::SDL_MOUSEBUTTONUP:
        xdo_mouse_up(xdo_, CURRENTWINDOW, e.button.button);
        break;
    case SDL_EventType::SDL_MOUSEWHEEL:
        xdo_click_window(xdo_, CURRENTWINDOW, e.wheel.y > 0 ? 4 : 5);
        break;
    // TODO: clipboard support
    // TODO: input method support
    /* case SDL_EventType::SDL_TEXTEDITING: */
    /* case SDL_EventType::SDL_TEXTEDITING_EXT: */
    /* case SDL_EventType::SDL_TEXTINPUT: */
    /*     logger::debug("recv textinput: {}", e.text.text); */
    /*     xdo_enter_text_window(xdo_, CURRENTWINDOW, &e.text.text[0], */
    /*                           kDelayMicros); */
    /*     break; */
    case SDL_EventType::SDL_KEYDOWN:
        seq = translate(e.key);
        xdo_send_keysequence_window_down(xdo_, CURRENTWINDOW, seq.c_str(),
                                         kDelayMicros);
        // FIXME: handle last down event
        break;
    case SDL_EventType::SDL_KEYUP:
        seq = translate(e.key);
        // non mod keys only trigger keyup event unless on long pressing
        xdo_send_keysequence_window_up(xdo_, CURRENTWINDOW, seq.c_str(),
                                       kDelayMicros);
        break;
    default:
        break;
    }
    return true;
}

#endif
