#include "peer_client.hh"
#include "logger.hh"
#include "video_capturer.hh"

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

static const std::string kDefaultSTUNServer = "stun:stun.xten.com";
static const std::string kAudioLabel = "audio_label";
static const std::string kVideoLabel = "video_label";
static const std::string kStreamId = "stream_id";

using SignalingState = webrtc::PeerConnectionInterface::SignalingState;

PeerClient::PeerClient() {}

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
        // TODO: log
        logger::error("failed to add audio track");
    }

    auto video_src = ScreenCapturer::Create();
    auto video_track =
        pc_factory_->CreateVideoTrack(kVideoLabel, video_src.get());

    if (video_sink_) {
        video_track->AddOrUpdateSink(video_sink_, rtc::VideoSinkWants());
    }

    result = pc_->AddTrack(video_track, {kStreamId});
    if (!result.ok()) {
        // TODO: log
        logger::error("failed to add video track");
    }
}

void PeerClient::addLocalSinks(
    rtc::VideoSinkInterface<webrtc::VideoFrame> *sink)
{
    video_sink_ = sink;
}

void PeerClient::addRemoteSinks(
    rtc::VideoSinkInterface<webrtc::VideoFrame> *sink)
{
    remote_sink_ = sink;
}

void PeerClient::OnMessage(MessageType mt, const std::string &payload)
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

// start a call, steps:
// makeCall
// CreateOffer
// -> SetLocalDescription
// -> send offer
// wait for answer
// onMessage(answer)
// SetRemoteDescription
// done
void PeerClient::makeCall()
{
    logger::debug("start calling");
    is_caller_ = true;
    assert(pc_->signaling_state() ==
           webrtc::PeerConnectionInterface::SignalingState::kStable);
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
    pc_->CreateOffer(this, opts);
}

// recieve a call, steps:
// onMessage(offer)
// onRemoteOffer
// SetRemoteDescription
// -> CreateAnswer
// -> SetLocalDescription
// -> send answer
// done
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
        logger::error("failed parse add candidate: {}", err.description);
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
    std::string sdp;
    desc->ToString(&sdp);

    if (is_caller_) {
        offer_ = sdp;
    } else {
        answer_ = sdp;
    }

    logger::debug("create local offer/answer: {}", sdp);

    // desc is the only owner, should be safe
    auto pdesc = std::unique_ptr<webrtc::SessionDescriptionInterface>(desc);
    pc_->SetLocalDescription(std::move(pdesc),
                             SetLocalSDPCallback::Create(this));
}

void PeerClient::OnFailure(webrtc::RTCError error)
{
    logger::error("create local offer/answer failed: {}", error.message());
}

void PeerClient::SetRemoteSDPCallback::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error)
{
    if (that_->is_caller_) { // remote answer set done
        /* assert(that_->pc_->signaling_state() ==
         * SignalingState::kHaveRemotePrAnswer); */
    } else { // remote offer set done
        assert(that_->pc_->signaling_state() ==
               SignalingState::kHaveRemoteOffer);
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
        that_->pc_->CreateAnswer(that_, opts);
    }
}

void PeerClient::SetLocalSDPCallback::OnSetLocalDescriptionComplete(
    webrtc::RTCError error)
{
    if (that_->is_caller_) { // CreateOffer callback
        that_->signaling_observer_->SendMessage(MessageType::kOffer,
                                                that_->offer_);
    } else { // CreateAnswer callback
        that_->signaling_observer_->SendMessage(MessageType::kAnswer,
                                                that_->answer_);
    }
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
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){

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
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel){};

void PeerClient::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
    if (!candidate) {
        logger::error("OnIceCandidate error, candidate is null");
        return;
    }
    std::string candi;
    candidate->ToString(&candi);
    candi_ = candi;

    logger::debug("got local candidate:\n{}", candi);

    signaling_observer_->SendMessage(MessageType::kCandidate, candi);
}

void PeerClient::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
}
