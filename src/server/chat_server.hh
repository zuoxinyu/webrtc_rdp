#pragma once

#include <map>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "peer.hh"

/**
 * PROTOCOL:
 * sign_in:
 *   POST /sign_in
 *   {name, id}
 * sign_out:
 *   POST /sign_out?id=$id
 * send_to:
 *   POST /send
 *   {to, msg}
 * wait:
 *   GET /wait?<id>
 */

using namespace boost;

struct ChatServer {
  public:
    using Id = Peer::Id;
    class IdGenerator
    {
        mutable int i = 0;

      public:
        Id next() const
        {
            i++;
            return std::to_string(i);
        };
    };

  public:
    ChatServer(asio::io_context &ctx, const std::string &host = "localhost",
               int port = 8888);
    ~ChatServer();

  public:
    asio::awaitable<void> do_listen();

  private:
    asio::awaitable<void> do_session(beast::tcp_stream);
    beast::http::message_generator
    handle_request(beast::http::request<beast::http::string_body> &&);
    beast::http::message_generator
    handle_sign_in(beast::http::request<beast::http::string_body> &&);
    beast::http::message_generator
    handle_sign_out(beast::http::request<beast::http::string_body> &&);
    beast::http::message_generator
    handle_send_to(beast::http::request<beast::http::string_body> &&);
    beast::http::message_generator
    handle_wait(beast::http::request<beast::http::string_body> &&);

    std::string peers_string() const;

  private:
    asio::io_context &ctx_;
    asio::ip::tcp::acceptor acceptor_;
    int port_;
    IdGenerator id_gen_;

    Peer::List peers_;
};
