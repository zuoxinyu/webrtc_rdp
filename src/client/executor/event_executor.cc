#include "event_executor.hh"

// TODO: need local/remote resolutions
auto EventExecutor::create(int w, int h, int rw, int rh)
    -> std::unique_ptr<EventExecutor>
{
#ifdef _WIN32
    return std::make_unique<Win32EventExecutor>(w, h, rw, rh);
#elif __linux__
    return std::make_unique<X11EventExecutor>(w, h, rw, rh);
#else
    // not implemented
    return nullptr;
#endif
}

EventExecutor::EventExecutor(int w, int h, int rw, int rh)
    : window_width_(w), window_height_(h), remote_width_(rw), remote_height_(rh)
{
}
