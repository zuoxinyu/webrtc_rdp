#include "event_executor.hh"

// TODO: need local/remote resolutions
auto EventExecutor::create(EventExecutor::WindowHandle handle)
    -> std::unique_ptr<EventExecutor>
{
#ifdef _WIN32
    return std::make_unique<Win32EventExecutor>(
        reinterpret_cast<SDL_Window *>(handle));
#elif __linux__
    return std::make_unique<X11EventExecutor>(
        reinterpret_cast<SDL_Window *>(handle));
#else
    // not implemented
    return nullptr;
#endif
}

EventExecutor::EventExecutor(SDL_Window *win) : win_(win)
{
    SDL_GetWindowSize(win_, &width_, &height_);
}
