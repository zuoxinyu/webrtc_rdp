#include "peer_client.hh"
#include "logger.hh"

#include <memory>
#include <utility>

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
static const std::string kRemoteVideoLabel = "remote_video_label";
static const std::string kStreamId = "stream_id";
static const std::string kDataChanId = "data_chan";

using SignalingState = webrtc::PeerConnectionInterface::SignalingState;

PeerClient::PeerClient()
{
    mq_ = std::make_unique<MessageQueue>();
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
}

PeerClient::~PeerClient() { deletePeerConnection(); }

bool PeerClient::createPeerConnection()
{
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer server;

    server.uri = kDefaultSTUNServer;
    config.servers.push_back(server);
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    // config.network_preference = rtc::AdapterType::ADAPTER_TYPE_WIFI;

    webrtc::PeerConnectionDependencies deps(this);
    auto err_or_pc =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(deps));

    if (err_or_pc.ok()) {
        pc_ = err_or_pc.MoveValue();
    }

    return pc_ != nullptr;
}

void PeerClient::deletePeerConnection()
{
    local_chan_->Close();
    pc_->Close();
    pc_ = nullptr;
    mq_ = std::make_unique<MessageQueue>();
    is_caller_ = false;

    if (local_video_src_ &&
        local_video_src_->state() == VideoSource::SourceState::kLive)
        local_video_src_->Stop();
    if (local_sink_)
        local_sink_->Stop();
    if (remote_video_src_ && remote_video_src_->state() == VideoSource::kLive)
        remote_video_src_->Stop();
    if (remote_sink_)
        remote_sink_->Stop();
}

void PeerClient::createTransceivers()
{
    auto transceiver =
        pc_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
    if (!transceiver.ok()) {
        logger::error("failed to add transceiver: {}",
                      transceiver.error().message());
    }
    transceiver = pc_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO);
    if (!transceiver.ok()) {
        logger::error("failed to add transceiver: {}",
                      transceiver.error().message());
    }

    if (remote_sink_) {
        remote_sink_->Start();
    }
}

void PeerClient::createLocalTracks()
{
    auto audio_src = pc_factory_->CreateAudioSource(cricket::AudioOptions());
    auto audio_track =
        pc_factory_->CreateAudioTrack(kAudioLabel, audio_src.get());
    auto result = pc_->AddTrack(audio_track, {kStreamId});
    if (!result.ok()) {
        logger::error("failed to add audio track");
    }

    if (local_video_src_) {
        local_video_src_->Start();
        auto local_video_track = pc_factory_->CreateVideoTrack(
            kLocalVideoLabel, local_video_src_.get());
        if (local_sink_) {
            local_video_track->AddOrUpdateSink(local_sink_.get(),
                                               rtc::VideoSinkWants());
            local_sink_->Start();
        }
    }

    if (remote_video_src_) {
        remote_video_src_->Start();
        // the scoped_refptr version will throw a weird `bad_alloc`, bug?
        auto remote_video_track = pc_factory_->CreateVideoTrack(
            kRemoteVideoLabel, remote_video_src_.get());
        result = pc_->AddTrack(remote_video_track, {kStreamId});
        if (!result.ok()) {
            logger::error("failed to add remote video track");
        }
    }
}

void PeerClient::createDataChannel()
{
    webrtc::DataChannelInit config{.protocol = "x11-rdp"};
    auto data_chan = pc_->CreateDataChannelOrError(kDataChanId, &config);
    if (data_chan.ok()) {
        local_chan_ = data_chan.value();
    } else {
        logger::error("failed to add data channel");
    }
}

void PeerClient::addLocalVideoSource(VideoSourcePtr src)
{
    local_video_src_ = std::move(src);
}

void PeerClient::addRemoteVideoSource(VideoSourcePtr src)
{
    remote_video_src_ = std::move(src);
}

void PeerClient::addLocalSinks(VideoSinkPtr sink)
{
    local_sink_ = std::move(sink);
}

void PeerClient::addRemoteSinks(VideoSinkPtr sink)
{
    remote_sink_ = std::move(sink);
}

void PeerClient::OnSignal(MessageType mt, const std::string &payload)
{
    switch (mt) {
    case MessageType::kUnknown:
        break;
    case MessageType::kReady:
        onReady();
        break;
    case MessageType::kOffer:
        onRemoteOffer(payload);
        break;
    case MessageType::kAnswer:
        onRemoteAnswer(payload);
        break;
    case MessageType::kCandidate:
        onRemoteCandidate(payload);
        break;
    case MessageType::kBye:
        onRemoteBye();
        break;
    case MessageType::kLogout:
        onDisconnect();
        break;
    }
}

void PeerClient::onReady()
{
    // pc_->RestartIce();
    logger::info("start calling");
    is_caller_ = true;

    // we don't send stream as caller
    createPeerConnection();
    createTransceivers();
    createDataChannel();

    assert(pc_->signaling_state() ==
           webrtc::PeerConnectionInterface::SignalingState::kStable);
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
    pc_->SetLocalDescription(SetLocalSDPCallback::Create(this));
}

void PeerClient::onRemoteOffer(const std::string &sdp)
{
    logger::debug("got remote offer:\n{}", sdp);
    is_caller_ = false;

    createPeerConnection();
    createLocalTracks();
    createDataChannel();

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
    assert(isCaller());
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

void PeerClient::onRemoteBye()
{
    logger::debug("remote send a disconnect");
    deletePeerConnection();
}

void PeerClient::onDisconnect()
{
    logger::debug("disconnect from server and peer");
    deletePeerConnection();
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

void PeerClient::OnTrack(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
    auto track = transceiver->receiver()->track().release();
    logger::debug("OnTrack: {}", track->id());
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto video_track = static_cast<webrtc::VideoTrackInterface *>(track);
        if (remote_sink_) {
            rtc::VideoSinkWants wants;
            video_track->AddOrUpdateSink(remote_sink_.get(),
                                         rtc::VideoSinkWants());
        }
    }
}

void PeerClient::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
    auto track = receiver->track().release();
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto video_track = static_cast<webrtc::VideoTrackInterface *>(track);
        if (remote_sink_) {
            video_track->RemoveSink(remote_sink_.get());
        }
    }
}

void PeerClient::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
    logger::debug("new remote channel [id={} proto={}] connected",
                  data_channel->id(), data_channel->protocol());
    remote_chan_ = data_channel;
    remote_chan_->RegisterObserver(this);
}

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
    }
    mq_->push(PeerClient::ChanMessage{msg.data.data(), msg.size(), msg.binary});
}

bool PeerClient::postTextMessage(const std::string &text)
{
    if (!local_chan_)
        return false;
    webrtc::DataBuffer blob(text);
    return local_chan_->Send(blob);
}

bool PeerClient::postBinaryMessage(const uint8_t *data, size_t size)
{
    if (!local_chan_)
        return false;
    rtc::CopyOnWriteBuffer buf(data, size);
    webrtc::DataBuffer blob{buf, true};
    return local_chan_->Send(blob);
}

std::optional<PeerClient::ChanMessage> PeerClient::pollRemoteMessage()
{
    if (mq_->empty())
        return {};

    ChanMessage m = mq_->front();
    mq_->pop();

    return m;
}
