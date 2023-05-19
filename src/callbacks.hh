#pragma once
#ifndef CALLBACKS_HH_
#define CALLBACKS_HH_

#include <peer.hh>

#include <string>

// TODO: pranswer/rollback?
enum class MessageType {
    kUnknown,
    kReady,
    kOffer,
    kAnswer,
    kCandidate,
    kBye,
    kLogout,
};

static inline MessageType MessageTypeFromString(const std::string &s)
{
    if (s == "ready")
        return MessageType::kReady;
    if (s == "offer")
        return MessageType::kOffer;
    if (s == "answer")
        return MessageType::kAnswer;
    if (s == "candidate")
        return MessageType::kCandidate;
    if (s == "bye")
        return MessageType::kBye;
    if (s == "logout")
        return MessageType::kLogout;

    return MessageType::kUnknown;
}

static inline std::string MessageTypeToString(MessageType v)
{
    switch (v) {
    case MessageType::kUnknown:
        return "unknown";
    case MessageType::kReady:
        return "ready";
    case MessageType::kOffer:
        return "offer";
    case MessageType::kAnswer:
        return "answer";
    case MessageType::kCandidate:
        return "candidate";
    case MessageType::kBye:
        return "bye";
    case MessageType::kLogout:
        return "logout";
    }
}

struct SignalingObserver {
    virtual void SendSignal(MessageType, const std::string &) = 0;
};

struct PeerObserver {
    virtual void OnSignal(MessageType, const std::string &) = 0;
};

struct UIObserver {
    virtual void OnLogin(Peer me) = 0;
    virtual void OnLogout(Peer me) = 0;
    virtual void OnPeersChanged(Peer::List peers) = 0;
};

#endif // CALLBACKS_HH_
