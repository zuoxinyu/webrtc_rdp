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
    static auto create(WindowHandle) -> std::unique_ptr<EventExecutor>;

  public:
    virtual ~EventExecutor() = default;

    virtual auto execute(Event) -> bool = 0;
};

#ifdef __WINDOWS__
class Win32Executor : public EventExecutor
{
  public:
    Win32Executor(SDL_Window *win) : win_(win)
    {
        SDL_GetWindowSize(win_, &width_, &height_);
    }
    ~Win32Executor() override = default;

    auto execute(EventExecutor::Event ev) -> bool override { return false; }

  private:
    struct Pos {
        int x;
        int y;
    };

  private:
    SDL_Window *win_;
    int width_ = 0;
    int height_ = 0;
    int remote_width_ = 2560;
    int remote_height_ = 1600;
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
    struct Pos {
        int x;
        int y;
    };
    auto translate(int x, int y) -> Pos;

    auto translateKeyseq(const SDL_Event &e, char key_seq[32]) -> void;

  private:
    xdo_t *xdo_;
    SDL_Window *win_;
    int width_ = 0;
    int height_ = 0;
    int remote_width_ = 2560;
    int remote_height_ = 1600;
};
#endif
