#pragma once

#include <memory>

#include <SDL2/SDL_events.h>

class EventExecutor
{
  public:
    struct Event {
        SDL_Event native_ev;
    };
    using WindowHandle = void *;
    struct Pos {
        int x;
        int y;
    };
    static auto create(WindowHandle) -> std::unique_ptr<EventExecutor>;

  public:
    EventExecutor(SDL_Window *win);
    virtual ~EventExecutor() = default;
    virtual auto execute(Event) -> bool = 0;

  protected:
    SDL_Window *win_;
    int width_ = 0;
    int height_ = 0;
    int remote_width_ = 2560;
    int remote_height_ = 1600;
};

#ifdef _WIN32 
class Win32EventExecutor : public EventExecutor
{
  public:
    Win32EventExecutor(SDL_Window *win);
    ~Win32EventExecutor() override = default;

    auto execute(EventExecutor::Event ev) -> bool override;
  private:
    int desktop_width;
    int desktop_height;
};
#endif

#ifdef __linux__
using xdo_t = struct xdo;
class X11EventExecutor : public EventExecutor
{
  public:
    X11EventExecutor(SDL_Window *win);
    ~X11EventExecutor() override;

    auto execute(EventExecutor::Event ev) -> bool override;

  private:
    auto translate(int x, int y) -> Pos;
    auto translateKeyseq(const SDL_Event &e, char key_seq[32]) -> void;

    xdo_t *xdo_;
};
#endif
