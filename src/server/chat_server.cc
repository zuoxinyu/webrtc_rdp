#include "server/chat_server.hh"

#include <algorithm>
#include <boost/asio/detached.hpp>
#include <boost/json/serializer.hpp>
#include <boost/url/parse.hpp>
#include <iostream>
#include <memory>
#include <set>

#include <boost/json.hpp>
#include <boost/url.hpp>

namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;
namespace asio = boost::asio;

using response = beast::http::response<beast::http::string_body>;
using asio::detached;
using asio::use_awaitable;

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

static std::set<beast::http::verb> supportMethods = {
    beast::http::verb::get,
    beast::http::verb::post,
    beast::http::verb::head,
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

    std::cout << "listening on: " << addr << std::endl;
}

ChatServer::~ChatServer() {}

asio::awaitable<void> ChatServer::do_listen()
{

    for (;;) {
        auto stream = beast::tcp_stream(
            co_await acceptor_.async_accept(ctx_, asio::use_awaitable));

        std::cout << "new connection from ["
                  << stream.socket().remote_endpoint() << "] " << std::endl;
        asio::co_spawn(ctx_, handle_http_session(std::move(stream)),
                       [](const std::exception_ptr &e) {
                           try {
                               std::rethrow_exception(e);
                           } catch (const std::exception &err) {
                               std::cerr << "Error in session: " << err.what()
                                         << "\n";
                           }
                       });
    }
}

asio::awaitable<void> ChatServer::handle_http_session(beast::tcp_stream stream)
{
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    for (;;)
        try {
            // Set the timeout.
            stream.expires_never();

            // Read a request
            http::request<http::string_body> req;

            co_await http::async_read(stream, buffer, req, use_awaitable);

            if (websocket::is_upgrade(req)) {
                websocket::stream<beast::tcp_stream> ws(
                    stream.release_socket());
                asio::co_spawn(ctx_, handle_websocket_session(std::move(ws)),
                               detached);
                co_return;
            }

            // Handle the request
            http::message_generator msg = handle_request(std::move(req));

            // Determine if we should close the connection
            bool keep_alive = msg.keep_alive();

            // Send the response
            co_await beast::async_write(stream, std::move(msg), use_awaitable);

            if (!keep_alive) {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                break;
            }
        } catch (boost::system::system_error &se) {
            if (se.code() != http::error::end_of_stream)
                throw;
        }

    // Send a TCP shutdown
    stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

asio::awaitable<void>
ChatServer::handle_websocket_session(websocket::stream<beast::tcp_stream> ws)
{
    co_return;
}

beast::http::message_generator
ChatServer::handle_request(beast::http::request<beast::http::string_body> &&req)
{
    std::cout << "request: " << req << std::endl;
    if (supportMethods.find(req.method()) == supportMethods.end()) {
        return response{beast::http::status::not_implemented, 11,
                        jsonErrorMethods};
    }
    auto url = urls::parse_origin_form(req.target());

    if (url->path() == "/sign_in") {
        return handle_sign_in(std::move(req));
    } else if (url->path() == "/sign_out") {
        return handle_sign_out(std::move(req));
    } else if (url->path() == "/send") {
        return handle_send_to(std::move(req));
    } else if (url->path() == "/wait") {
        return handle_wait(std::move(req));
    }

    return response{beast::http::status::not_found, req.version()};
}

beast::http::message_generator
ChatServer::handle_sign_in(beast::http::request<beast::http::string_body> &&req)
{

    std::cout << "body:" << req.body() << std::endl;
    Peer peer{json::parse(req.body())};
    peer.online = true;
    peer.id = id_gen_.next();
    PeerState state{peer, MessageQueue()};

    peers_.insert_or_assign(peer.id, state);

    auto resp = response{beast::http::status::ok, req.version()};
    resp.set(beast::http::field::pragma, peer.id);
    resp.set(beast::http::field::content_type, "text/json");
    resp.keep_alive(true);
    resp.body() = json::serialize(peers_json());
    resp.prepare_payload();

    return resp;
}

beast::http::message_generator ChatServer::handle_sign_out(
    beast::http::request<beast::http::string_body> &&req)
{
    Peer peer{json::parse(req.body())};
    peers_.erase(peer.id);
    auto resp = response{beast::http::status::ok, req.version()};
    resp.set(beast::http::field::pragma, peer.id);
    resp.keep_alive(false);

    return resp;
}

beast::http::message_generator
ChatServer::handle_wait(beast::http::request<beast::http::string_body> &&req)
{
    auto params = urls::parse_origin_form(req.target())->params();
    auto id_param = params.find("peer_id");
    if (id_param == params.end()) {
        return response{beast::http::status::bad_request, 11};
    }
    std::string id = (*id_param).value;
    auto &msgq = peers_[id].msg_queue;
    json::object msg_body;
    if (!msgq.empty()) {
        std::string pending_msg = peers_[id].msg_queue.front();
        peers_[id].msg_queue.pop();
        msg_body = {{"peers", peers_json()}, {"msg", json::parse(pending_msg)}};
    } else {
        msg_body = {{"peers", peers_json()}};
    }

    auto resp = response{beast::http::status::ok, 11};
    resp.set(beast::http::field::pragma, id);
    resp.set(beast::http::field::content_type, "text/json");
    resp.keep_alive(true);
    resp.body() = json::serialize(msg_body);
    resp.prepare_payload();

    return resp;
}

beast::http::message_generator
ChatServer::handle_send_to(beast::http::request<beast::http::string_body> &&req)
{
    json::value body = json::parse(req.body());
    json::object obj = body.get_object();
    json::string to = obj["to"].get_string();
    json::object msg = obj["msg"].get_object();

    peers_[std::string(to)].msg_queue.push(json::serialize(msg));

    auto params = urls::parse_origin_form(req.target())->params();
    auto id_param = params.find("peer_id");
    if (id_param == params.end()) {
        return response{beast::http::status::bad_request, 11};
    }
    std::string id = (*id_param).value;
    auto resp = response{beast::http::status::ok, 11};
    resp.set(beast::http::field::pragma, id);
    resp.keep_alive(true);

    resp.body() = "ok";
    resp.prepare_payload();

    return resp;
}

json::array ChatServer::peers_json() const
{
    json::array peers_json;
    for (auto &it : peers_) {
        peers_json.push_back(json::value(it.second.peer));
    }
    return peers_json;
}
