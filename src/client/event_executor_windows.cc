#ifdef _WIN32
#include "event_executor.hh"
#include "logger.hh"

#include <memory>

#include <winuser.h>

#include <SDL2/SDL_events.h>

static constexpr int kDelayMicros = 12000;

static auto translate(const SDL_MouseMotionEvent &e) -> INPUT
{
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi = {
        .dx = e.x,
        .dy = e.y,
        .mouseData = 0,
        .dwFlags = MOUSEEVENTF_ABSOLUTE,
        .time = 0,
        .dwExtraInfo = 0,
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
    input.mi = {.dx = e.x,
                .dy = e.y,
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

auto Win32EventExecutor::execute(EventExecutor::Event ev) -> bool
{
    SDL_Event e = ev.native_ev;
    INPUT inputs[1] = {0};
    UINT sent;
    switch (e.type) {
    case SDL_EventType::SDL_MOUSEMOTION:
        logger::debug("windows recv motion: {},{}", e.motion.x, e.motion.y);
        inputs[0] = translate(e.motion);
        // TODO: bypass UIPI
        SendInput(1, inputs, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_MOUSEBUTTONDOWN:
        inputs[0] = translate(e.motion);
        SendInput(1, inputs, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_MOUSEBUTTONUP:
        inputs[0] = translate(e.motion);
        SendInput(1, inputs, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_MOUSEWHEEL:
        inputs[0] = translate(e.motion);
        SendInput(1, inputs, sizeof(INPUT));
        break;
    case SDL_EventType::SDL_KEYDOWN:
        break;
    case SDL_EventType::SDL_KEYUP:
        break;
    default:
        break;
    }
    return true;
}

#endif
