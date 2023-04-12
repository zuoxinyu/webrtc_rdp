#include "event_executor.hh"

#include <memory>

#include <SDL2/SDL_events.h>
#include <xdo.h>

class X11EventExecutor : public EventExecutor
{
  public:
    X11EventExecutor() : xdo_() { xdo_.reset(xdo_new("")); }
    ~X11EventExecutor() override { xdo_free(xdo_.get()); }

    auto execute(EventExecutor::Event ev) -> bool override
    {
        SDL_Event e = ev.native_ev;
        switch (e.type) {
        case SDL_EventType::SDL_MOUSEMOTION:
            xdo_move_mouse(xdo_.get(), e.motion.x, e.motion.y, 0);
            break;
        case SDL_EventType::SDL_MOUSEBUTTONDOWN:
            xdo_mouse_down(xdo_.get(), CURRENTWINDOW, e.button.button);
            break;
        case SDL_EventType::SDL_MOUSEBUTTONUP:
            xdo_mouse_up(xdo_.get(), CURRENTWINDOW, e.button.button);
            break;
        default:
            break;
        }
        return true;
    }

  private:
    auto translate() -> EventExecutor::Event;

  private:
    std::unique_ptr<xdo_t> xdo_;
};

auto EventExecutor::create() -> std::unique_ptr<EventExecutor>
{
    return std::make_unique<X11EventExecutor>();
}
