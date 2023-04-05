#pragma once

#include "peer.hh"

#include <memory>
#include <ranges>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

using namespace boost::asio;
using boost::asio::awaitable;
using boost::system::error_code;

namespace beast = boost::beast;
namespace http = beast::http;

// the clien entity, run on a dedicated network thread
struct ChatClient {
  public:
    explicit ChatClient(io_context &ctx);
    ChatClient(const ChatClient &) = delete;
    ChatClient(ChatClient &&) = delete;
    ChatClient &operator=(const ChatClient &) = delete;
    ChatClient &operator=(ChatClient &&) = delete;
    ~ChatClient();

  public:
    // impl UIObserver
    awaitable<void> StartLogin(std::string server, int port);
    awaitable<void> DisconnectFromServer();
    awaitable<void> ConnectToPeer(int peer_id);
    awaitable<void> DisconnectFromCurrentPeer();
    awaitable<void> ListenOnPeerList();

  public:
    bool is_signed() const { return me_.online; }
    const Peer &me() const { return me_; }
    const Peer::List &peers() const { return peers_; }
    const Peer::List online_peers() const;
    const Peer::List offline_peers() const;
    Peer::Id current_peer() const { return current_peer_; }

  private:
    void set_peers(const json::value &v);

  private:
    // TODO: dedicate thread
    std::unique_ptr<std::thread> thread_;
    // io executor
    io_context &ctx_;
    //  socket connected to signal server
    beast::tcp_stream stream_;
    asio::steady_timer wait_timer_;
    ip::tcp::endpoint server_;
    // states
    Peer::List peers_;
    Peer::Id current_peer_ = "-1";
    Peer me_ = {"my_name", "-1", false};
    std::string read_buffer_;
};
