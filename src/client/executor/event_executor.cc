#include "event_executor.hh"

#ifdef __linux__
#include <xdo.h>
#endif

// TODO: need local/remote resolutions
auto EventExecutor::create(int w, int h, int rw, int rh)
    -> std::unique_ptr<EventExecutor>
{
    return std::make_unique<EventExecutor>(w, h, rw, rh);
}

EventExecutor::EventExecutor(int w, int h, int rw, int rh)
    : window_width_(w), window_height_(h), remote_width_(rw), remote_height_(rh)
{
#ifdef __linux__
    xdo_ = xdo_new(nullptr);
#endif
}
