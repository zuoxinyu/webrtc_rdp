#pragma once

#include "callbacks.hh"
#include "peer.hh"

#include <memory>
#include <queue>
#include <ranges>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <utility>

namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;

using asio::awaitable;
using asio::detached;
using asio::io_context;
using asio::use_awaitable;
using boost::system::error_code;

// the client entity, should run on a dedicated network thread
struct ChatClient : public SignalingObserver {
  public:
    struct PendingMessage {
        MessageType mt;
        std::string payload;
    };

  public:
    explicit ChatClient(io_context &ctx, const std::string &name = "my_name");
    ~ChatClient();

  public:
    awaitable<void> signin(std::string server, int port);
    awaitable<void> logout();
    awaitable<void> sendMessage(Peer::Id id, std::string offer, MessageType mt);
    awaitable<void> waitMessage();
    awaitable<void> handlePendingMessages();

  public:
    bool isSigned() const { return me_.online; }
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
    void SendMessage(MessageType, const std::string &) override;

  private:
    void set_peers(const json::value &v);

  private:
    // TODO: dedicate thread
    std::unique_ptr<std::thread> thread_;
    // io executor
    io_context &ctx_;
    io_context::strand strand_;
    beast::tcp_stream stream_;
    beast::tcp_stream wait_stream_;
    asio::steady_timer wait_timer_;
    asio::steady_timer send_timer_;
    asio::ip::tcp::endpoint server_;
    PeerObserver *peer_observer_;
    // states
    std::string name_;
    Peer::List peers_;
    Peer::Id current_peer_ = "-1";
    Peer me_ = {name_, "-1", false};
    std::queue<PendingMessage> pending_messages_;
};
