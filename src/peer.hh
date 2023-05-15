#pragma once

#include <map>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;

struct Peer {
    std::string name;
    std::string id;
    bool online;

    using Id = std::string;
    using List = std::map<Id, Peer>;

    Peer() = default;

    Peer(std::string name, std::string id, bool online)
        : name(std::move(name)), id(std::move(id)), online(online)
    {
    }

    Peer(const json &v)
    {
        name = v["name"];
        id = v["id"];
        online = v["online"];
    }

    ~Peer() = default;

    Peer &operator=(json &&v)
    {
        Peer x = Peer(v);
        std::swap(*this, x);
        return *this;
    }

    operator json() const
    {
        return {
            {"name", name},
            {"id", id},
            {"online", online},
        };
    }

    operator std::string()
    {
        json v = {
            {"name", name},
            {"id", id},
            {"online", online},
        };
        return v.dump();
    }

    bool operator==(const Peer &other) const { return name == other.name; }

    struct Less {
        bool operator()(const Peer &a, const Peer &b) const { return a == b; }
    };
};
