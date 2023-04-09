#include "chat_client.hh"
#include "logger.hh"

#include <algorithm>
#include <boost/asio/bind_executor.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <utility>

#include <boost/json.hpp>

template <bool B, typename T, typename P>
static std::string to_str(const http::message<B, T, P> &resp)
{
    std::stringstream ss;
    ss << resp;
    return ss.str();
}

ChatClient::ChatClient(io_context &ctx, const std::string &name)
    : thread_(nullptr), ctx_(ctx), strand_(ctx_), stream_(ctx_),
      wait_stream_(ctx_), wait_timer_(ctx_, std::chrono::seconds(30)),
      send_timer_(ctx_, std::chrono::seconds(1)),
      server_(asio::ip::make_address_v4("127.0.0.1"), 8888), name_(name)
{
}

ChatClient::~ChatClient()
{
    stream_.close();
    wait_stream_.close();
    wait_timer_.cancel();
    send_timer_.cancel();
}

// impl UIObserver
awaitable<void> ChatClient::signin(std::string host, int port)
{
    logger::info("Login to {}:{}", host, port);
    server_ = asio::ip::tcp::endpoint(asio::ip::make_address_v4(host), port);

    beast::flat_buffer fb;
    http::response<http::string_body> resp;
    std::string target = "/sign_in";
    http::request<http::string_body> req{http::verb::post, target, 11, me_};
    req.keep_alive(true);
    req.set(http::field::host, server_.address().to_string());
    req.set(http::field::connection, "keep-alive");
    req.set(http::field::content_type, "text/json");
    req.prepare_payload();
    logger::debug("signin request: \n{}", to_str(req));

    try {
        stream_.expires_never();
        co_await stream_.async_connect(server_, use_awaitable);
        co_await http::async_write(stream_, req, use_awaitable);
        co_await http::async_read(stream_, fb, resp, use_awaitable);
        logger::debug("server response:\n{}", to_str(resp));

        me_.id = resp["Pragma"];
        me_.online = true;

        set_peers(json::parse(resp.body()));
    } catch (beast::system_error &se) {
        if (se.code() == http::error::end_of_stream) {
            logger::error("login failed: server closed");
        } else if (se.code() == asio::error::connection_refused) {
            logger::error("login failed: connectioin refused");
        } else {
            logger::error("login failed: {}", se.what());
        }
        co_return;
    }

    co_spawn(ctx_, waitMessage(), detached);
    /* co_spawn(ctx_, handlePendingMessages(), detached); */
}

awaitable<void> ChatClient::waitMessage()
{
    try {
        logger::debug("start wait message thread");
        wait_stream_.expires_never();
        co_await wait_stream_.async_connect(server_, use_awaitable);
        while (isSigned()) {
            while (!pending_messages_.empty()) {
                logger::debug("pending messages: {}", pending_messages_.size());
                auto m = pending_messages_.front();
                pending_messages_.pop();
                co_await sendMessage(currentPeer(), m.payload, m.mt);
            }

            std::string target = "/wait?peer_id=" + me_.id;
            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, server_.address().to_string());
            req.set(http::field::connection, "keep-alive");
            /* logger::debug("wait request:\n{}", to_str(req)); */

            http::response<http::string_body> resp;
            beast::flat_buffer fb;

            co_await http::async_write(wait_stream_, req, use_awaitable);
            co_await http::async_read(wait_stream_, fb, resp, use_awaitable);

            /* logger::debug("wait response:\n{}", to_str(resp)); */
            json::object body = json::parse(resp.body()).as_object();

            set_peers(body["peers"]);

            if (body["msg"].is_object()) {
                json::object msg = body["msg"].as_object();
                std::string peer_id = std::string(msg["from"].as_string());
                MessageType mt =
                    MessageTypeFromString(std::string(msg["type"].as_string()));
                std::string payload(msg["payload"].get_string());

                if (mt == MessageType::kOffer) {
                    setCurrentPeer(peer_id);
                }
                peer_observer_->OnMessage(mt, payload);
            }
            wait_timer_.expires_after(std::chrono::seconds(1));
            co_await wait_timer_.async_wait(use_awaitable);
        }
    } catch (const std::exception &e) {
        logger::error("wait error: {}", e.what());
    }
}

awaitable<void> ChatClient::logout()
{
    stream_.close();
    wait_stream_.close();
    wait_timer_.cancel();
    send_timer_.cancel();
    me_.online = false;
    co_return;
}

awaitable<void> ChatClient::sendMessage(Peer::Id peer_id, std::string payload,
                                        MessageType mt)
{
    logger::debug("send to peer [id={}, type={}]", peer_id,
                  MessageTypeToString(mt));

    std::string target = "/send?peer_id=" + me_.id;
    http::request<http::string_body> req{http::verb::post, target, 11};
    req.set(http::field::host, server_.address().to_string());
    req.set(http::field::connection, "keep-alive");
    req.set(http::field::content_type, "text/json");

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

    logger::debug("send request:\n{}", to_str(req));
    try {
        http::response<http::string_body> resp;
        beast::flat_buffer fb;

        co_await http::async_write(wait_stream_, req, use_awaitable);
        co_await http::async_read(wait_stream_, fb, resp, use_awaitable);
        logger::debug("send response:\n{}", to_str(resp));
    } catch (beast::error_code &ec) {
        logger::error("send failed: {}", ec.what());
    }
}

awaitable<void> ChatClient::handlePendingMessages()
{
    logger::debug("start handle pending message thread");
    while (isSigned()) {
        while (!pending_messages_.empty()) {
            logger::debug("pending messages: {}", pending_messages_.size());
            auto m = pending_messages_.front();
            pending_messages_.pop();
            co_await sendMessage(currentPeer(), m.payload, m.mt);
        }
        send_timer_.expires_after(std::chrono::seconds(1));
        co_await send_timer_.async_wait(use_awaitable);
    }
    logger::debug("handle pending messages quit");
}

void ChatClient::set_peers(const json::value &v)
{
    json::array peers = v.as_array();
    std::for_each(peers.begin(), peers.end(), [this](const Peer &v) {
        peers_.insert({v.id, v});
    });
}

const Peer::List ChatClient::onlinePeers() const
{
    Peer::List online_peers;
    for (auto &p : peers_) {
        if (p.second.online) {
            online_peers.insert(p);
        }
    }
    return online_peers;
};

const Peer::List ChatClient::offlinePeers() const
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
void ChatClient::SendMessage(MessageType mt, const std::string &msg)
{
    pending_messages_.push({mt, msg});
}
