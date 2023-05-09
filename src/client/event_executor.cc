#include "event_executor.hh"

// TODO: need local/remote resolutions
auto EventExecutor::create(EventExecutor::WindowHandle handle)
    -> std::unique_ptr<EventExecutor>
{
#ifdef __WINDOWS__
    return std::make_unique<Win32Executor>(
        reinterpret_cast<SDL_Window *>(handle));
#else
    return std::make_unique<X11EventExecutor>(
        reinterpret_cast<SDL_Window *>(handle));
#endif
}
