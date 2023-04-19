#pragma once

#include "callbacks.hh"
#include "peer.hh"

#include <memory>
#include <queue>
#include <ranges>
#include <string>
#include <thread>
#include <utility>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;

using asio::awaitable;
using asio::detached;
using asio::io_context;
using asio::use_awaitable;
using boost::system::error_code;

// the client entity, should run on a dedicated network thread
struct SignalClient : public SignalingObserver {
  public:
    struct PendingMessage {
        Peer::Id id;
        MessageType mt;
        std::string payload;
    };

    struct Config {
        std::string name;
        std::string host;
        int port;
        bool use_websocket;
        std::chrono::duration<long> stream_expire_time;
        std::chrono::duration<long> http_wait_time;
    };

    static constexpr Config kDefaultConfig{
        "user",
        "127.0.0.1",
        8888,
        false,
        std::chrono::seconds(30),
        std::chrono::seconds(1),
    };

  public:
    explicit SignalClient(io_context &ctx, Config conf = kDefaultConfig);
    ~SignalClient();

  public:
    void set_peer_observer(PeerObserver *observer)
    {
        peer_observer_ = observer;
    }
    void login(const std::string &server, int port);
    void logout();
    bool online() const { return me_.online; }
    bool calling() const { return current_peer_.has_value(); }
    const std::string id() const { return me_.id; }
    const std::string name() const { return me_.name; }
    void set_name(std::string name) { me_.name = std::move(name); }
    const Peer::Id current() const { return current_peer_.value(); }
    const Peer &peer() const { return peers_.at(current()); }
    void start_session(Peer::Id peer_id);
    void stop_session();
    const Peer::List &peers() const { return peers_; }
    const Peer::List online_peers() const;
    const Peer::List offline_peers() const;

  public:
    void SendSignal(MessageType, const std::string &) override;

  private:
    awaitable<void> signin(std::string server, int port);
    awaitable<void> signout();
    awaitable<void> send_message(Peer::Id id, std::string offer,
                                 MessageType mt);
    awaitable<void> wait_message();
    awaitable<void> handle_pending_messages();

  private:
    void set_peers(const json::value &v);
    void do_logout();

  private:
    // TODO: dedicate thread
    std::unique_ptr<std::thread> thread_ = nullptr;
    // io executor
    io_context &ctx_;
    // beast::websocket::stream<beast::tcp_stream> ws_;
    beast::tcp_stream stream_;
    asio::steady_timer wait_timer_;
    PeerObserver *peer_observer_;

    // states
    Config conf_;
    Peer::List peers_;
    std::optional<Peer::Id> current_peer_;
    Peer me_ = {kDefaultConfig.name, "-1", false};
    std::queue<PendingMessage> pending_messages_;
};
