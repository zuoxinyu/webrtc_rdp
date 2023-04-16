#include "signal_client.hh"
#include "client/callbacks.hh"
#include "logger.hh"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>
#include <utility>

#include <boost/json.hpp>

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

SignalClient::~SignalClient() { doLogout(); }

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

        setPeers(json::parse(resp.body()));
    } catch (beast::system_error &se) {
        if (se.code() == http::error::end_of_stream) {
            logger::error("login failed: server closed");
        } else if (se.code() == asio::error::connection_refused) {
            logger::error("login failed: connectioin refused");
        } else {
            logger::error("login failed: {}", se.what());
        }
        doLogout();
        co_return;
    }

    co_spawn(ctx_, waitMessage(), detached);
}

awaitable<void> SignalClient::waitMessage()
{
    logger::debug("start wait message thread");
    auto server =
        asio::ip::tcp::endpoint(make_address_v4(conf_.host), conf_.port);
    try {
        stream_.expires_never();
        co_await stream_.async_connect(server, use_awaitable);
        while (online()) {
            co_await handlePendingMessages();

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
            json::object body = json::parse(resp.body()).as_object();

            setPeers(body["peers"]);

            if (body["msg"].is_object()) {
                json::object msg = body["msg"].as_object();
                std::string peer_id = std::string(msg["from"].as_string());
                MessageType mt =
                    MessageTypeFromString(std::string(msg["type"].as_string()));
                std::string payload(msg["payload"].get_string());

                if (mt == MessageType::kOffer) {
                    startSession(peer_id);
                }
                peer_observer_->OnSignal(mt, payload);
            }
            co_await wait_timer_.async_wait(use_awaitable);
        }
    } catch (beast::system_error &se) {
        if (se.code() == http::error::end_of_stream) {
            logger::error("login failed: server closed");
        } else if (se.code() == asio::error::connection_refused) {
            logger::error("login failed: connectioin refused");
        } else {
            logger::error("login failed: {}", se.what());
        }
        doLogout();
        co_return;
    }
}

awaitable<void> SignalClient::signout()
{
    doLogout();
    co_return;
}

void SignalClient::doLogout()
{
    stream_.close();
    wait_timer_.cancel();
    me_.online = false;
}

awaitable<void> SignalClient::sendMessage(Peer::Id peer_id, std::string payload,
                                          MessageType mt)
{
    logger::trace("send to peer [id={}, type={}]", peer_id,
                  MessageTypeToString(mt));

    std::string target = "/send?peer_id=" + me_.id;
    http::request<http::string_body> req{http::verb::post, target, 11};
    req.set(http::field::host, conf_.host);
    req.set(http::field::content_type, "text/json");
    req.set(http::field::pragma, me_.id);

    // clang-formag off
    json::object msg = {{"to", peer_id},
                        {"msg",
                         {
                             {"from", me_.id},
                             {"type", MessageTypeToString(mt)},
                             {"payload", payload},
                         }}};
    // clang-formag on

    req.body() = json::serialize(msg);
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

awaitable<void> SignalClient::handlePendingMessages()
{
    // handle pending messages
    while (!pending_messages_.empty()) {
        auto m = pending_messages_.front();
        pending_messages_.pop();
        co_await sendMessage(current(), m.payload, m.mt);
    }
};

void SignalClient::setPeers(const json::value &v)
{
    json::array peers = v.as_array();
    peers_.clear();

    std::for_each(peers.begin(), peers.end(), [this](const Peer &v) {
        peers_.insert({v.id, v});
    });

    if (calling() && peers_.find(current()) == peers_.end()) {
        logger::info("current peer logout or closed: {}", current());
        peer_observer_->OnSignal(MessageType::kBye, "bye");
        current_peer_ = {};
    }
}

const Peer::List SignalClient::onlinePeers() const
{
    Peer::List online_peers;
    for (auto &p : peers_) {
        if (p.second.online) {
            online_peers.insert(p);
        }
    }
    return online_peers;
};

const Peer::List SignalClient::offlinePeers() const
{
    Peer::List offline_peers;
    for (auto &p : peers_) {
        if (!p.second.online) {
            offline_peers.insert(p);
        }
    }
    return offline_peers;
};

// SignalingObserver
void SignalClient::SendSignal(MessageType mt, const std::string &msg)
{
    pending_messages_.push({mt, msg});
}

void SignalClient::stopSession()
{
    SendSignal(MessageType::kBye, "bye");
    peer_observer_->OnSignal(MessageType::kLogout, "logout");
    current_peer_ = {};
}

void SignalClient::startSession(Peer::Id peer_id)
{
    peer_observer_->OnSignal(MessageType::kReady, "ready");
    current_peer_ = std::move(peer_id);
}
