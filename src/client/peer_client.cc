#include "peer_client.hh"
#include "logger.hh"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "rtc_base/thread.h"

static const std::string kDefaultSTUNServer = "stun:stun1.l.google.com:19302";
static const std::string kAudioLabel = "audio_label";
static const std::string kLocalVideoLabel = "local_video_label";
static const std::string kRemoteVideoLabel = "local_video_label";
static const std::string kStreamId = "stream_id";
static const std::string kDataChanId = "data_chan";

using SignalingState = webrtc::PeerConnectionInterface::SignalingState;

PeerClient::PeerClient() : mq_(std::make_unique<MessageQueue>()) {}

bool PeerClient::createPeerConnection()
{
    signaling_thread_ = rtc::Thread::CreateWithSocketServer();
    signaling_thread_->Start();

    pc_factory_ = webrtc::CreatePeerConnectionFactory(
        nullptr, nullptr, signaling_thread_.get(), nullptr,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);

    webrtc::PeerConnectionFactoryInterface::Options factory_opts;
    // disable_encryption would make data_channel create failed, bug?
    /* factory_opts.disable_encryption = true; */
    pc_factory_->SetOptions(factory_opts);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer server;

    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    server.uri = kDefaultSTUNServer;
    config.servers.push_back(server);

    webrtc::PeerConnectionDependencies pc_deps(this);
    auto err_or_pc =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(pc_deps));

    if (err_or_pc.ok()) {
        pc_ = std::move(err_or_pc.value());
    }

    return pc_ != nullptr;
}

void PeerClient::deletePeerConnection()
{
    pc_ = nullptr;
    pc_factory_ = nullptr;
}

void PeerClient::createLocalTracks()
{
    if (!pc_->GetSenders().empty()) {
        return; // already added
    }

    auto audio_src = pc_factory_->CreateAudioSource(cricket::AudioOptions());
    auto audio_track =
        pc_factory_->CreateAudioTrack(kAudioLabel, audio_src.get());
    auto result = pc_->AddTrack(audio_track, {kStreamId});
    if (!result.ok()) {
        logger::error("failed to add audio track");
    }

    if (local_video_src_) {
        auto local_video_track =
            pc_factory_->CreateVideoTrack(kLocalVideoLabel, local_video_src_);
        if (local_sink_) {
            local_video_track->AddOrUpdateSink(local_sink_,
                                               rtc::VideoSinkWants());
        }
    }

    auto remote_video_track =
        pc_factory_->CreateVideoTrack(kRemoteVideoLabel, remote_video_src_);
    result = pc_->AddTrack(remote_video_track, {kStreamId});
    if (!result.ok()) {
        logger::error("failed to add remote video track");
    }
}

void PeerClient::addLocalVideoSource(VideoSourcePtr src)
{
    local_video_src_ = src;
}
void PeerClient::addRemoteVideoSource(VideoSourcePtr src)
{
    remote_video_src_ = src;
}

void PeerClient::addLocalSinks(VideoSinkPtr sink) { local_sink_ = sink; }

void PeerClient::addRemoteSinks(VideoSinkPtr sink) { remote_sink_ = sink; }

void PeerClient::OnSignal(MessageType mt, const std::string &payload)
{
    switch (mt) {
    case MessageType::kUnknown:
        break;
    case MessageType::kReady:
        break;
    case MessageType::kOffer:
        is_caller_ = false;
        onRemoteOffer(payload);
        break;
    case MessageType::kAnswer:
        assert(is_caller_);
        onRemoteAnswer(payload);
        break;
    case MessageType::kCandidate:
        onRemoteCandidate(payload);
        break;
    case MessageType::kBye:
        break;
    }
}

void PeerClient::makeCall()
{
    // pc_->RestartIce();
    logger::info("start calling");

    webrtc::DataChannelInit config{.protocol = "x11-rdp"};
    auto data_chan = pc_->CreateDataChannelOrError(kDataChanId, &config);
    if (data_chan.ok()) {
        local_chan_ = data_chan.value();
    } else {
        logger::error("failed to add data channel");
    }

    is_caller_ = true;
    assert(pc_->signaling_state() ==
           webrtc::PeerConnectionInterface::SignalingState::kStable);
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
    pc_->SetLocalDescription(SetLocalSDPCallback::Create(this));
}

void PeerClient::onRemoteOffer(const std::string &sdp)
{
    logger::debug("got remote offer:\n{}", sdp);
    assert(pc_->signaling_state() ==
           webrtc::PeerConnectionInterface::SignalingState::kStable);
    webrtc::SdpParseError err;
    auto offer =
        webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp, &err);
    pc_->SetRemoteDescription(std::move(offer),
                              SetRemoteSDPCallback::Create(this));
}

void PeerClient::onRemoteAnswer(const std::string &sdp)
{
    logger::debug("got remote answer:\n{}", sdp);
    assert(pc_->signaling_state() == SignalingState::kHaveLocalOffer);
    webrtc::SdpParseError err;
    auto offer =
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &err);
    pc_->SetRemoteDescription(std::move(offer),
                              SetRemoteSDPCallback::Create(this));
}

void PeerClient::onRemoteCandidate(const std::string &sdp)
{
    logger::debug("got remote candidate:\n{}", sdp);
    webrtc::SdpParseError err;
    std::unique_ptr<webrtc::IceCandidateInterface> candi;
    candi.reset(webrtc::CreateIceCandidate("", 0, sdp, &err));

    if (!candi) {
        logger::error("failed parse remote candidate: {}", err.description);
        return;
    }

    pc_->AddIceCandidate(std::move(candi), [](const webrtc::RTCError &e) {
        if (!e.ok()) {
            logger::error("failed to add candidate: {}", e.message());
        }
    });
}

// impl CreateSessionDescriptorInterface

void PeerClient::OnSuccess(webrtc::SessionDescriptionInterface *desc)
{
    logger::debug("create local offer/answer: {}", desc->type());
}

void PeerClient::OnFailure(webrtc::RTCError error)
{
    logger::error("create local offer/answer failed: {}", error.message());
}

void PeerClient::SetRemoteSDPCallback::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error)
{
    if (!error.ok()) {
        logger::error("failed to set remote description: {}", error.message());
        return;
    }

    if (!that_->isCaller()) {
        that_->pc_->SetLocalDescription(SetLocalSDPCallback::Create(that_));
    }
}

void PeerClient::SetLocalSDPCallback::OnSetLocalDescriptionComplete(
    webrtc::RTCError error)
{
    if (!error.ok()) {
        logger::error("failed to set local description: {}", error.message());
        return;
    }

    std::string sdp;
    auto desc = that_->pc_->local_description();
    desc->ToString(&sdp);

    logger::debug("set local {}:\n{}", desc->type(), sdp);

    auto mt = that_->isCaller() ? MessageType::kOffer : MessageType::kAnswer;
    that_->signaling_observer_->SendSignal(mt, sdp);
}

// PeerConnectionObserver impl

void PeerClient::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    auto vs = stream->GetVideoTracks();
    if (vs.size() > 0) {
        vs[0]->AddOrUpdateSink(remote_sink_, rtc::VideoSinkWants());
    }
};

void PeerClient::OnRemoveStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    auto vs = stream->GetVideoTracks();
    for (auto &v : vs) {
        v->RemoveSink(remote_sink_);
    }
};

void PeerClient::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
        &streams)
{
    auto track = receiver->track().release();
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto video_track = static_cast<webrtc::VideoTrackInterface *>(track);
        if (remote_sink_) {
            rtc::VideoSinkWants wants;
            video_track->AddOrUpdateSink(remote_sink_, rtc::VideoSinkWants());
        }
    }
};

void PeerClient::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
    auto track = receiver->track().release();
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto video_track = static_cast<webrtc::VideoTrackInterface *>(track);
        if (remote_sink_) {
            video_track->RemoveSink(remote_sink_);
        }
    }
};

void PeerClient::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
    logger::debug("new remote channel [id={} proto={}] connected",
                  data_channel->id(), data_channel->protocol());
    remote_chan_ = data_channel;
    remote_chan_->RegisterObserver(this);
};

void PeerClient::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
    if (!candidate) {
        logger::error("OnIceCandidate got null candidate");
        return;
    }
    std::string candi;
    candidate->ToString(&candi);

    logger::debug("got local candidate:\n{}", candi);

    signaling_observer_->SendSignal(MessageType::kCandidate, candi);
}

void PeerClient::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
}

void PeerClient::OnIceSelectedCandidatePairChanged(
    const cricket::CandidatePairChangeEvent &event)
{
    logger::debug(
        "ICE selected candidate changed, reason:{}\nlocal: {}\nremote: {}",
        event.reason,
        event.selected_candidate_pair.local_candidate().ToString(),
        event.selected_candidate_pair.remote_candidate().ToString());
}

// DataChannelInterface impl
void PeerClient::OnMessage(const webrtc::DataBuffer &msg)
{
    if (!msg.binary) {
        std::string m(reinterpret_cast<const char *>(msg.data.data()),
                      msg.size());
        logger::debug("recv remote text message: {}", m);
    } else {
        /* logger::debug("recv remote binary message: {}", msg.size()); */
    }
    mq_->push(PeerClient::ChanMessage{msg.data.data(), msg.size(), msg.binary});
}

bool PeerClient::sendTextMessage(const std::string &text)
{
    if (!local_chan_)
        return false;
    webrtc::DataBuffer blob(text);
    return local_chan_->Send(blob);
}

bool PeerClient::sendBinaryMessage(const uint8_t *data, size_t size)
{
    if (!local_chan_)
        return false;
    rtc::CopyOnWriteBuffer buf(data, size);
    webrtc::DataBuffer blob{buf, true};
    return local_chan_->Send(blob);
}

std::optional<PeerClient::ChanMessage> PeerClient::recvMessage()
{
    if (mq_->empty())
        return {};

    ChanMessage m = mq_->front();
    mq_->pop();

    return m;
}
