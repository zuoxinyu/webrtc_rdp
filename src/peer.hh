#pragma once

#include <map>
#include <string>

#include <boost/json.hpp>
#include <boost/json/serialize.hpp>
using namespace boost;

#include <string>
#include <utility>
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

    Peer(const json::value &json)
    {
        name = json.at("name").get_string();
        id = json.at("id").get_string();
        online = json.at("online").get_bool();
    }

    ~Peer() = default;

    Peer &operator=(json::value &&v)
    {
        Peer x = Peer(v);
        std::swap(*this, x);
        return *this;
    }

    operator json::value() const
    {
        return json::object{
            {"name", name},
            {"id", id},
            {"online", online},
        };
    }

    operator std::string() { return json::serialize(*this); }

    bool operator==(const Peer &other) const { return name == other.name; }

    struct Less {
        bool operator()(const Peer &a, const Peer &b) const { return a == b; }
    };
};
