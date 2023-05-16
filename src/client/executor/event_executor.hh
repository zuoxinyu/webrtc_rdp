#pragma once

#include <memory>

#include <SDL2/SDL_events.h>

class EventExecutor
{
  public:
#ifdef __linux__
    using xdo_t = struct xdo;
#endif
    struct Event {
        SDL_Event native_ev;
    };
    struct Pos {
        int x;
        int y;
    };
    static auto create(int w, int h, int rw, int rh)
        -> std::unique_ptr<EventExecutor>;

  public:
    EventExecutor(int w, int h, int rw, int rh);
    ~EventExecutor() = default;
    auto execute(Event) -> bool;
    // auto mouse_move();
    // auto mouse_click();
    // auto key_down();
    // auto key_up();

  protected:
    int window_width_ = 0;
    int window_height_ = 0;
    int remote_width_ = 0;
    int remote_height_ = 0;

  private:
#ifdef __linux__
    xdo_t *xdo_;
#endif
};
