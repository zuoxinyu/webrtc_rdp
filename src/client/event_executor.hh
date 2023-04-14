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
