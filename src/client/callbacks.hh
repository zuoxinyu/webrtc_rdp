#pragma once
#ifndef CALLBACKS_HH_
#define CALLBACKS_HH_

#include <string>

enum class MessageType {
    kUnknown,
    kReady,
    kOffer,
    kAnswer,
    kCandidate,
    kBye,
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
    }
}

struct SignalingObserver {
    virtual void SendSignal(MessageType, const std::string &) = 0;
};

struct PeerObserver {
    virtual void OnSignal(MessageType, const std::string &) = 0;
};

#endif // CALLBACKS_HH_
