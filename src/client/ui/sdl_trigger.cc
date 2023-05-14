#include "sdl_trigger.hh"
#include <algorithm>
#include <iostream>
#include <utility>

namespace Trigger
{

std::vector<Group *> groups;
Group globalGroup;

bool KeyCombination::hasKey(SDL_Keycode key) const
{
    for (auto i : keys) {
        if (i.key == key) {
            return true;
        }
    }

    return false;
}

void KeyCombination::markKeyDown(SDL_Keycode key)
{
    for (auto &i : keys) {
        if (i.key == key) {
            i.isDown = true;
        }
    }
}

void KeyCombination::markKeyUp(SDL_Keycode key)
{
    for (auto &i : keys) {
        if (i.key == key) {
            i.isDown = false;
        }
    }
}

void KeyCombination::reset()
{
    for (auto &key : keys) {
        key.isDown = false;
    }
}

bool KeyCombination::isFulfilled() const
{
    for (auto key : keys) {
        if (!key.isDown) {
            return false;
        }
    }

    return true;
}

Trigger::Trigger(const Keycodes &keys, Callback callback)
    : callback{std::move(callback)}
{
    for (const auto key : keys) {
        combination.keys.push_back({key, false});
    }
}

Group::Group() : triggers{}, isEnabled{true} { groups.push_back(this); }

Group::~Group()
{
    groups.erase(std::remove(groups.begin(), groups.end(), this));
}

void Group::enable() { isEnabled = true; }

void Group::disable()
{
    isEnabled = false;

    for (auto &trigger : triggers) {
        trigger.combination.reset();
    }
}

void Group::toggle()
{
    if (isEnabled) {
        disable();
    } else {
        enable();
    }
}

void Group::on(SDL_Keycode &&key, Callback callback)
{
    on(Keycodes{key}, std::move(callback));
}

void Group::on(Keycodes &&keys, Callback callback)
{
    triggers.emplace_back(std::move(keys), std::move(callback));
}

void Group::processEvent(SDL_Event &e)
{
    SDL_Keycode key = e.key.keysym.sym;

    if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
        for (auto &trigger : triggers) {
            if (trigger.combination.hasKey(key)) {
                trigger.combination.markKeyDown(key);
            } else {
                trigger.combination.reset();
            }
        }

        for (auto &trigger : triggers) {
            if (trigger.combination.isFulfilled()) {
                trigger.callback();
            }
        }
    } else if (e.type == SDL_KEYUP) {
        for (auto &trigger : triggers) {
            if (trigger.combination.hasKey(key)) {
                trigger.combination.markKeyUp(key);
            }
        }
    }
}

void on(SDL_Keycode &&key, Callback callback)
{
    on(Keycodes{key}, std::move(callback));
}

void on(Keycodes &&keys, Callback callback)
{
    globalGroup.triggers.emplace_back(std::move(keys), std::move(callback));
}

// TODO: add return value indicates event is fullfilled
void processEvent(SDL_Event &e)
{
    for (auto &group : groups) {
        if (group->isEnabled) {
            group->processEvent(e);
        }
    }
}

} // namespace Trigger
