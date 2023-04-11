#include "server/chat_server.hh"
#include "logger.hh"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>
#include <memory>
#include <set>

#include <boost/asio/error.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/json.hpp>
#include <boost/json/serializer.hpp>
#include <boost/url.hpp>
#include <boost/url/parse.hpp>

using namespace std::chrono_literals;

template <bool B, typename T, typename P>
static std::string to_str(const http::message<B, T, P> &resp)
{
    std::stringstream ss;
    ss << resp;
    return ss.str();
}

static std::string jsonErrorMethods = R"(
{
    "err": "only support HEAD|GET|POST"
}
)";

static std::string jsonErrorInvalid = R"(
{
    "err": "invalid path"
}
)";

static std::set<http::verb> supportMethods = {
    http::verb::get,
    http::verb::post,
    http::verb::head,
};

static std::set<std::string> supportPath = {"/sign_in", "/sign_out", "/send",
                                            "/online"};

ChatServer::ChatServer(asio::io_context &ctx, const std::string &host, int port)
    : ctx_(ctx), acceptor_(ctx), port_(port)
{
    auto addr = asio::ip::tcp::endpoint{asio::ip::make_address(host),
                                        static_cast<unsigned short>(port)};
    acceptor_.open(addr.protocol());

    // Allow address reuse
    acceptor_.set_option(asio::socket_base::reuse_address(true));

    // Bind to the server address
    acceptor_.bind(addr);

    // Start listening for connections
    acceptor_.listen(asio::socket_base::max_listen_connections);

    logger::info("listening on: {}:{}", host, port);
}

ChatServer::~ChatServer() = default;

auto ChatServer::run() -> asio::awaitable<void>
{

    for (;;)
    {
        auto stream = std::make_unique<beast::tcp_stream>(
            co_await acceptor_.async_accept(ctx_, asio::use_awaitable));

        logger::info("new connection from [{}:{}]",
                     stream->socket().remote_endpoint().address().to_string(),
                     stream->socket().remote_endpoint().port());
        asio::co_spawn(ctx_, handle_http_session(std::move(stream)),
                       [](const std::exception_ptr &e)
                       {
                           try
                           {
                               if (e)
                                   std::rethrow_exception(e);
                           }
                           catch (const std::exception &err)
                           {
                               logger::error("Error in session: {}",
                                             err.what());
                           }
                       });
    }
}

auto ChatServer::handle_http_session(std::shared_ptr<beast::tcp_stream> stream)
    -> asio::awaitable<void>
{
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    std::string id;

    // This lambda is used to send messages
    for (;;)
    {
        try
        {
            // Set the timeout.
            stream->expires_after(30s);

            // Read a request
            request req;

            co_await http::async_read(*stream, buffer, req, use_awaitable);

            id = req[http::field::pragma];

            if (websocket::is_upgrade(req))
            {
                auto ws = make_unique<wstream>(stream->release_socket());
                co_await asio::co_spawn(
                    ctx_,
                    handle_websocket_session(std::move(ws), std::move(req)),
                    use_awaitable);
                co_return;
            }

            // Handle the request
            http::message_generator msg = handle_request(std::move(req));

            // Determine if we should close the connection
            bool keep_alive = msg.keep_alive();

            // Send the response
            co_await beast::async_write(*stream, std::move(msg), use_awaitable);

            if (!keep_alive)
            {
                break;
            }
        }
        catch (boost::system::system_error &se)
        {
            if (se.code() == asio::error::network_reset ||
                se.code() == asio::error::connection_reset ||
                se.code() == http::error::end_of_stream)
            {
                logger::info("peer[{}], connection closed", id);
            }
            else
            {
                logger::info("peer[{}], connection closed due to {}", id,
                             se.what());
            }
            break;
        }
    }

    if (!id.empty())
    {
        peers_.erase(id);
    }

    // Send a TCP shutdown
    stream->socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

auto ChatServer::handle_websocket_session(std::shared_ptr<wstream> ws,
                                          request req) -> asio::awaitable<void>
{
    ws->set_option(
        websocket::stream_base::decorator([](websocket::response_type &resp)
                                          { resp.set(http::field::server, "chat-server"); }));
    co_await ws->async_accept(req, use_awaitable);

    beast::flat_buffer fb;
    auto nbytes = co_await ws->async_read(fb, use_awaitable);
    std::string msg = beast::buffers_to_string(fb.data());

    co_return;
}

auto ChatServer::handle_request(request &&req) -> http::message_generator
{
    logger::debug("request: \n{}", to_str(req));
    if (supportMethods.find(req.method()) == supportMethods.end())
    {
        return response{http::status::not_implemented, 11, jsonErrorMethods};
    }
    auto url = urls::parse_origin_form(req.target());

    if (url->path() == "/sign_in")
    {
        return handle_sign_in(std::move(req));
    }
    else if (url->path() == "/sign_out")
    {
        return handle_sign_out(std::move(req));
    }
    else if (url->path() == "/send")
    {
        return handle_send_to(std::move(req));
    }
    else if (url->path() == "/wait")
    {
        return handle_wait(std::move(req));
    }

    return response{http::status::not_found, req.version()};
}

auto ChatServer::handle_sign_in(request &&req) -> http::message_generator
{
    Peer peer{json::parse(req.body())};
    peer.online = true;
    peer.id = id_gen_.next();
    PeerState state{peer, MessageQueue()};

    peers_.insert_or_assign(peer.id, state);

    auto resp = response{http::status::ok, req.version()};
    resp.set(http::field::pragma, peer.id);
    resp.set(http::field::content_type, "text/json");
    resp.keep_alive(req.keep_alive());
    resp.body() = json::serialize(peers_json());
    resp.prepare_payload();

    return resp;
}

auto ChatServer::handle_sign_out(request &&req) -> http::message_generator
{
    Peer peer{json::parse(req.body())};
    peers_.erase(peer.id);
    auto resp = response{http::status::ok, req.version()};
    resp.set(http::field::pragma, peer.id);
    resp.keep_alive(req.keep_alive());
    resp.body() = "ok";
    resp.prepare_payload();

    return resp;
}

auto ChatServer::handle_wait(request &&req) -> http::message_generator
{
    std::string id = req[http::field::pragma];
    auto &msgq = peers_[id].msg_queue;
    json::object msg_body;
    if (!msgq.empty())
    {
        std::string pending_msg = peers_[id].msg_queue.front();
        peers_[id].msg_queue.pop();
        msg_body = {{"peers", peers_json()}, {"msg", json::parse(pending_msg)}};
    }
    else
    {
        msg_body = {{"peers", peers_json()}};
    }

    auto resp = response{http::status::ok, req.version()};
    resp.set(http::field::pragma, id);
    resp.set(http::field::content_type, "text/json");
    resp.keep_alive(true);
    resp.body() = json::serialize(msg_body);
    resp.prepare_payload();

    return resp;
}

auto ChatServer::handle_send_to(request &&req) -> http::message_generator
{
    json::value body = json::parse(req.body());
    json::object obj = body.get_object();
    json::string to = obj["to"].get_string();
    json::object msg = obj["msg"].get_object();

    peers_[std::string(to)].msg_queue.push(json::serialize(msg));

    auto resp = response{http::status::ok, req.version()};
    resp.set(http::field::pragma, req[http::field::pragma]);
    resp.keep_alive(req.keep_alive());

    resp.body() = "ok";
    resp.prepare_payload();

    return resp;
}

auto ChatServer::peers_json() const -> json::array
{
    json::array peers_json;
    for (auto &it : peers_)
    {
        peers_json.push_back(json::value(it.second.peer));
    }
    return peers_json;
}
