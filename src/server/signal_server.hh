#pragma once
#include "peer.hh"

#include <map>
#include <queue>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

using wstream = websocket::stream<beast::tcp_stream>;
using response = http::response<beast::http::string_body>;
using request = http::request<beast::http::string_body>;
using asio::detached;
using asio::use_awaitable;

struct SignalServer {
  public:
    struct PeerState;
    using PeerState = struct PeerState;
    using MessageQueue = std::queue<std::string>;
    using PeerMap = std::map<Peer::Id, PeerState>;

    class IdGenerator
    {
        mutable int i = 0;

      public:
        Peer::Id next() const
        {
            i++;
            return std::to_string(i);
        };
    };

    struct PeerState {
        Peer peer;
        MessageQueue msg_queue;
        std::shared_ptr<beast::tcp_stream> stream;
        std::shared_ptr<wstream> wstream;
    };

  public:
    SignalServer(asio::io_context &ctx, const std::string &host = "localhost",
                 int port = 8888);
    ~SignalServer();

  public:
    asio::awaitable<void> run();

  private:
    auto handle_http_session(std::shared_ptr<beast::tcp_stream>)
        -> asio::awaitable<void>;
    auto handle_websocket_session(std::shared_ptr<wstream>, request)
        -> asio::awaitable<void>;
    auto handle_request(request &&) -> response;
    auto handle_sign_in(request &&) -> response;
    auto handle_sign_out(request &&) -> response;
    auto handle_send_to(request &&) -> response;
    auto handle_wait(request &&) -> response;

    auto peers_json() const -> json;

  private:
    asio::io_context &ctx_;
    asio::ip::tcp::acceptor acceptor_;
    int port_;
    IdGenerator id_gen_;
    PeerMap peers_;
};
