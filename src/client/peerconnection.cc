#include "peerconnection.hh"

#include <iostream>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "rtc_base/thread.h"
#include "video_capturer.hh"

static const std::string kDefaultSTUNServer = "stun:10.10.10.190:3478";
static const std::string kAudioLabel = "audio_label";
static const std::string kVideoLabel = "video_label";
static const std::string kStreamId = "stream_id";

PeerConnectionImpl::PeerConnectionImpl()
    : signaling_thread_(nullptr), video_sink_(nullptr)
{
    /* webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options; */
    /* pc_->CreateOffer(this, options); */
    /* pc_->CreateAnswer(this, options); */
}

bool PeerConnectionImpl::createPeerConnection()
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

void PeerConnectionImpl::deletePeerConnection()
{
    pc_ = nullptr;
    pc_factory_ = nullptr;
}

void PeerConnectionImpl::addTracks()
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
        std::cerr << "failed to add audio track" << std::endl;
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
        std::cerr << "failed to add video track" << std::endl;
    }
}

void PeerConnectionImpl::addSinks(
    rtc::VideoSinkInterface<webrtc::VideoFrame> *sink)
{
    video_sink_ = sink;
}

void PeerConnectionImpl::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
        &streams){};
void PeerConnectionImpl::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver){};
void PeerConnectionImpl::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){};
void PeerConnectionImpl::OnRemoveStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){};
void PeerConnectionImpl::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel){};

// impl CreateSessionDescriptorInterface

void PeerConnectionImpl::OnSuccess(webrtc::SessionDescriptionInterface *desc)
{
    std::cout << "Session descriptor: \n" << desc << std::endl;
}

void PeerConnectionImpl::OnFailure(webrtc::RTCError error)
{
    std::cerr << "Create failed: " << error.message() << std::endl;
}
