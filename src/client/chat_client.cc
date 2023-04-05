#include "chat_client.hh"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include <boost/json.hpp>

namespace json = boost::json;

ChatClient::ChatClient(io_context &ctx)
    : thread_(nullptr), ctx_(ctx), stream_(ctx_),
      wait_timer_(ctx_, std::chrono::seconds(30)),
      server_(ip::make_address_v4("127.0.0.1"), 8888)
{
    thread_ = std::make_unique<std::thread>(
        [this] { std::cout << "ChatClient create" << std::endl; });
}

ChatClient::~ChatClient()
{
    stream_.close();
    wait_timer_.cancel();
}

// impl UIObserver
awaitable<void> ChatClient::StartLogin(std::string host, int port)
{
    std::cout << "Login to: " << host << ":" << port << std::endl;
    server_ = ip::tcp::endpoint(ip::make_address_v4(host), port);

    beast::flat_buffer fb;
    http::response<http::string_body> resp;
    std::string target = "/sign_in";
    http::request<http::string_body> req{http::verb::post, target, 11, me_};
    req.keep_alive(true);
    req.set(http::field::host, server_.address().to_string());
    req.set(http::field::connection, "keep-alive");
    req.set(http::field::content_type, "text/json");
    req.prepare_payload();
    std::cout << "request: " << req << std::endl;

    try {
        stream_.expires_never();
        co_await stream_.async_connect(server_, use_awaitable);
        co_await http::async_write(stream_, req, use_awaitable);
        co_await http::async_read(stream_, fb, resp, use_awaitable);
        std::cout << "server response: " << resp << std::endl;

        set_peers(json::parse(resp.body()));

        co_spawn(ctx_, ListenOnPeerList(), detached);
    } catch (const std::exception &err) {
        std::cerr << "login failed: " << err.what() << std::endl;
    }
}

awaitable<void> ChatClient::ListenOnPeerList()
{
    std::string target = "/wait?peer_id=" + me_.id;
    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, server_.address().to_string());
    req.set(http::field::connection, "keep-alive");
    std::cout << "wait request: " << req << std::endl;

    try {
        while (true) {
            std::cout << "waiting for server status" << std::endl;
            http::response<http::string_body> resp;
            beast::flat_buffer fb;
            stream_.expires_never();
            co_await http::async_write(stream_, req, use_awaitable);
            co_await http::async_read(stream_, fb, resp, use_awaitable);

            std::cout << "wait response" << resp << std::endl;
            set_peers(json::parse(resp.body()));

            wait_timer_.expires_from_now(std::chrono::seconds(30));
            co_await wait_timer_.async_wait(use_awaitable);
        }
    } catch (const std::exception &e) {
        std::cout << "wait error: " << e.what() << std::endl;
    }
}

awaitable<void> ChatClient::DisconnectFromServer()
{
    stream_.close();
    me_.online = false;
    co_return;
}

awaitable<void> ChatClient::ConnectToPeer(int peer_id) { co_return; }

awaitable<void> ChatClient::DisconnectFromCurrentPeer() { co_return; }

void ChatClient::set_peers(const json::value &v)
{
    json::array peers = v.as_array();
    auto it =
        std::find_if(peers.cbegin(), peers.cend(),
                     [this](const json::value &v) { return Peer(v) == me_; });

    me_ = Peer(*it);

    std::for_each(peers.begin(), peers.end(), [this](const Peer &v) {
        peers_.insert({v.id, v});
    });
}

const Peer::List ChatClient::online_peers() const
{
    Peer::List online_peers;
    for (auto &p : peers_) {
        if (p.second.online) {
            online_peers.insert(p);
        }
    }
    return online_peers;
};

const Peer::List ChatClient::offline_peers() const
{
    Peer::List offline_peers;
    for (auto &p : peers_) {
        if (!p.second.online) {
            offline_peers.insert(p);
        }
    }
    return offline_peers;
};
