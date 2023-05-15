#pragma once

#include <memory>

#include <SDL2/SDL_events.h>

class EventExecutor
{
  public:
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
    virtual ~EventExecutor() = default;
    virtual auto execute(Event) -> bool = 0;

  protected:
    int window_width_ = 0;
    int window_height_ = 0;
    int remote_width_ = 2560;
    int remote_height_ = 1600;
};

#ifdef _WIN32
class Win32EventExecutor : public EventExecutor
{
  public:
    Win32EventExecutor(int w, int h, int rw, int rh);
    ~Win32EventExecutor() override = default;

    auto execute(EventExecutor::Event ev) -> bool override;
};
#endif

#ifdef __linux__
using xdo_t = struct xdo;
class X11EventExecutor : public EventExecutor
{
  public:
    X11EventExecutor(int w, int h, int rw, int rh);
    ~X11EventExecutor() override;

    auto execute(EventExecutor::Event ev) -> bool override;

  private:
    auto translate() -> Pos;
    auto translateKeyseq(const SDL_Event &e, char key_seq[32]) -> void;

    xdo_t *xdo_;
};
#endif
