#include "signal_client.hh"
#include "callbacks.hh"
#include "logger.hh"

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <exception>
#include <iostream>
#include <thread>
#include <utility>

#include <boost/beast.hpp>

namespace http = beast::http;

using asio::ip::make_address_v4;

template <bool B, typename T, typename P>
static std::string to_str(const http::message<B, T, P> &resp)
{
    std::stringstream ss;
    ss << resp;
    return ss.str();
}

SignalClient::SignalClient(io_context &ctx, Config conf)
    : ctx_(ctx), stream_(ctx_), wait_timer_(ctx_, conf.http_wait_time),
      conf_(std::move(conf)), me_(conf_.name, "-1", false)
{
}

SignalClient::~SignalClient() { do_logout(); }

void SignalClient::login(const std::string &server, int port)
{
    co_spawn(ctx_, signin(server, port), detached);
}

void SignalClient::logout() { co_spawn(ctx_, signout(), detached); }

// impl UIObserver
awaitable<void> SignalClient::signin(std::string host, int port)
{
    logger::info("Login to {}:{}", host, port);

    conf_.host = host;
    conf_.port = port;

    auto server = asio::ip::tcp::endpoint(make_address_v4(host), port);
    beast::flat_buffer fb;
    http::response<http::string_body> resp;
    std::string target = "/sign_in";
    http::request<http::string_body> req{http::verb::post, target, 11, me_};
    req.keep_alive(true);
    req.set(http::field::host, conf_.host);
    req.set(http::field::content_type, "text/json");
    req.prepare_payload();

    logger::trace("signin request: \n{}", to_str(req));

    try {
        stream_.expires_never();
        co_await stream_.async_connect(server, use_awaitable);
        co_await http::async_write(stream_, req, use_awaitable);
        co_await http::async_read(stream_, fb, resp, use_awaitable);
        logger::trace("server response:\n{}", to_str(resp));

        me_.id = resp[http::field::pragma];
        me_.online = true;

        set_peers(json::parse(resp.body()));

        ui_observer_->OnLogin(me_);
    } catch (beast::system_error &se) {
        if (se.code() == http::error::end_of_stream) {
            logger::error("login failed: server closed");
        } else if (se.code() == asio::error::connection_refused) {
            logger::error("login failed: connectioin refused");
        } else {
            logger::error("login failed: {}", se.what());
        }
        do_logout();
        co_return;
    }

    co_spawn(ctx_, wait_message(), detached);
}

awaitable<void> SignalClient::wait_message()
{
    logger::debug("start wait message thread");
    auto server =
        asio::ip::tcp::endpoint(make_address_v4(conf_.host), conf_.port);
    try {
        // stream_.expires_never();
        // co_await stream_.async_connect(server, use_awaitable);
        while (online()) {
            co_await handle_pending_messages();

            std::string target = "/wait?peer_id=" + me_.id;
            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, conf_.host);
            req.set(http::field::pragma, me_.id);
            /* logger::debug("wait request:\n{}", to_str(req)); */

            http::response<http::string_body> resp;
            beast::flat_buffer fb;

            co_await http::async_write(stream_, req, use_awaitable);
            co_await http::async_read(stream_, fb, resp, use_awaitable);

            /* logger::debug("wait response:\n{}", to_str(resp)); */
            json body = json::parse(resp.body());

            set_peers(body["peers"]);

            if (body["msg"].is_object()) {
                json msg = body["msg"];
                auto peer_id = std::string(msg["from"]);
                auto payload = std::string(msg["payload"]);
                auto mt = MessageTypeFromString(msg["type"]);

                if (mt == MessageType::kOffer) {
                    current_peer_ = std::move(peer_id);
                }
                if (mt == MessageType::kBye) {
                    current_peer_ = {};
                }
                peer_observer_->OnSignal(mt, payload);
            }
            co_await wait_timer_.async_wait(use_awaitable);
        }
    } catch (beast::system_error &se) {
        if (se.code() == http::error::end_of_stream) {
            logger::error("wait failed: server closed");
        } else if (se.code() == asio::error::connection_refused) {
            logger::error("wait failed: connectioin refused");
        } else {
            logger::error("wait failed: {}", se.what());
        }
        do_logout();
        co_return;
    }
}

awaitable<void> SignalClient::signout()
{
    do_logout();
    co_return;
}

void SignalClient::do_logout()
{
    stream_.close();
    wait_timer_.cancel();
    me_.online = false;
    ui_observer_->OnLogout(me_);
}

awaitable<void> SignalClient::send_message(Peer::Id peer_id,
                                           std::string payload, MessageType mt)
{
    logger::trace("send to peer [id={}, type={}]", peer_id,
                  MessageTypeToString(mt));

    std::string target = "/send?peer_id=" + me_.id;
    http::request<http::string_body> req{http::verb::post, target, 11};
    req.set(http::field::host, conf_.host);
    req.set(http::field::content_type, "text/json");
    req.set(http::field::pragma, me_.id);

    json msg = {{"to", peer_id},
                {"msg",
                 {
                     {"from", me_.id},
                     {"type", MessageTypeToString(mt)},
                     {"payload", payload},
                 }}};

    req.body() = msg.dump();
    req.prepare_payload();

    logger::trace("send request:\n{}", to_str(req));
    try {
        http::response<http::string_body> resp;
        beast::flat_buffer fb;

        co_await http::async_write(stream_, req, use_awaitable);
        co_await http::async_read(stream_, fb, resp, use_awaitable);
        logger::trace("send response:\n{}", to_str(resp));
    } catch (beast::error_code &ec) {
        logger::error("send failed: {}", ec.what());
    }
}

awaitable<void> SignalClient::handle_pending_messages()
{
    // handle pending messages
    while (!pending_messages_.empty()) {
        auto m = pending_messages_.front();
        pending_messages_.pop();
        co_await send_message(m.id, m.payload, m.mt);
    }
};

void SignalClient::set_peers(const json &v)
{
    json peers = v;
    peers_.clear();

    std::for_each(peers.begin(), peers.end(), [this](const Peer &v) {
        peers_.insert({v.id, v});
    });

    if (calling() && peers_.find(current()) == peers_.end()) {
        logger::info("current peer logout or closed: {}", current());
        peer_observer_->OnSignal(MessageType::kBye, "bye");
        current_peer_ = {};
    }

    ui_observer_->OnPeersChanged(peers_);
}

// SignalingObserver
void SignalClient::SendSignal(MessageType mt, const std::string &msg)
{
    pending_messages_.push({current(), mt, msg});
}

void SignalClient::stop_session()
{
    // bug: sync issues
    SendSignal(MessageType::kBye, "bye");
    current_peer_ = {};
    peer_observer_->OnSignal(MessageType::kLogout, "logout");
}

void SignalClient::start_session(Peer::Id peer_id)
{
    current_peer_ = std::move(peer_id);
    peer_observer_->OnSignal(MessageType::kReady, "ready");
}
