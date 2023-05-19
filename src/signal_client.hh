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
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace asio = boost::asio;

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
        std::string name = "user";
        std::string host = "127.0.0.1";
        int port = 8888;
        bool use_websocket = false;
        std::chrono::duration<long> stream_expire_time =
            std::chrono::seconds(30);
        std::chrono::duration<long> http_wait_time = std::chrono::seconds(1);
    };

  public:
    explicit SignalClient(io_context &ctx, Config conf);
    ~SignalClient();

    void set_peer_observer(PeerObserver *observer)
    {
        peer_observer_ = observer;
    }

    void set_ui_observer_(UIObserver *observer) { ui_observer_ = observer; }

    void login(const std::string &server, int port);
    void logout();
    void start_session(Peer::Id peer_id);
    void stop_session();

    bool online() const { return me_.online; }
    bool calling() const { return current_peer_.has_value(); }
    const std::string id() const { return me_.id; }
    const std::string name() const { return me_.name; }
    void set_name(std::string name) { me_.name = std::move(name); }
    const Peer::Id current() const { return current_peer_.value(); }
    const Peer &peer() const { return peers_.at(current()); }
    const Peer::List &peers() const { return peers_; }

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
    void set_peers(const json &v);
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
    UIObserver *ui_observer_;

    // states
    Config conf_;
    Peer::List peers_;
    std::optional<Peer::Id> current_peer_;
    Peer me_ = {"user", "-1", false};
    std::queue<PendingMessage> pending_messages_;
};
