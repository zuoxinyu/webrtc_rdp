#include "event_executor.hh"
#include "logger.hh"

#include <memory>

#include <SDL2/SDL_events.h>
#include <xdo.h>

class X11EventExecutor : public EventExecutor
{
  public:
    X11EventExecutor(SDL_Window *win) : win_(win)
    {
        xdo_ = xdo_new(nullptr);
        SDL_GetWindowSize(win_, &width_, &height_);
    }
    ~X11EventExecutor() override { xdo_free(xdo_); }

    auto execute(EventExecutor::Event ev) -> bool override
    {
        SDL_Event e = ev.native_ev;
        int x, y, screen_num;
        Pos pos = {0, 0};
        xdo_get_mouse_location(xdo_, &x, &y, &screen_num);
        switch (e.type) {
        case SDL_EventType::SDL_MOUSEMOTION:
            pos = translate(e.motion.x, e.motion.y);
            xdo_move_mouse(xdo_, pos.x, pos.y, screen_num);
            break;
        case SDL_EventType::SDL_MOUSEBUTTONDOWN:
            xdo_mouse_down(xdo_, CURRENTWINDOW, e.button.button);
            break;
        case SDL_EventType::SDL_MOUSEBUTTONUP:
            xdo_mouse_up(xdo_, CURRENTWINDOW, e.button.button);
            break;
        default:
            break;
        }
        return true;
    }

  private:
    struct Pos {
        int x;
        int y;
    };
    auto translate(int x, int y) -> Pos
    {
        return Pos{static_cast<int>(
                       ((double)x / (double)width_ * (double)remote_width_)),
                   static_cast<int>((double)y / (double)height_ *
                                    (double)remote_height_)};
    }

  private:
    xdo_t *xdo_;
    SDL_Window *win_;
    int width_ = 0;
    int height_ = 0;
    int remote_width_ = 2560;
    int remote_height_ = 1600;
};

// TODO: need local/remote resolutions
auto EventExecutor::create(EventExecutor::WindowHandle handle)
    -> std::unique_ptr<EventExecutor>
{
    return std::make_unique<X11EventExecutor>(
        reinterpret_cast<SDL_Window *>(handle));
}
