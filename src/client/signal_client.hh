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
        "myname",
        "127.0.0.1",
        8888,
        false,
        std::chrono::seconds(30),
        std::chrono::seconds(1),
    };

  public:
    explicit SignalClient(io_context &ctx, Config conf = kDefaultConfig);
    explicit SignalClient(io_context &ctx, const std::string &name)
        : SignalClient(ctx, {name, kDefaultConfig.host, kDefaultConfig.port,
                             kDefaultConfig.use_websocket,
                             kDefaultConfig.stream_expire_time,
                             kDefaultConfig.http_wait_time})
    {
    }
    ~SignalClient();

  public:
    awaitable<void> signin(std::string server, int port);
    awaitable<void> logout();
    awaitable<void> sendMessage(Peer::Id id, std::string offer, MessageType mt);
    awaitable<void> waitMessage();
    awaitable<void> handlePendingMessages();

  public:
    bool online() const { return me_.online; }
    const Peer &me() const { return me_; }
    const Peer::Id currentPeer() const { return current_peer_; }
    void setCurrentPeer(Peer::Id peer_id)
    {
        current_peer_ = std::move(peer_id);
    }
    const Peer::List &peers() const { return peers_; }
    const Peer::List onlinePeers() const;
    const Peer::List offlinePeers() const;
    void setPeerObserver(PeerObserver *observer) { peer_observer_ = observer; }

  public:
    void SendSignal(MessageType, const std::string &) override;

  private:
    void setPeers(const json::value &v);

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
    Peer::Id current_peer_ = "-1";
    Peer me_ = {kDefaultConfig.name, "-1", false};
    std::queue<PendingMessage> pending_messages_;
};
