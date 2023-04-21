#include "peer_client.hh"
#include "api/video_codecs/video_encoder_factory.h"
#include "client/codec/decoder/factory.hh"
#include "client/codec/encoder/factory.hh"
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
#include "api/video_codecs/video_encoder_factory_template.h"
#include "rtc_base/thread.h"

static const std::string kDefaultSTUNServer = "stun:stun1.l.google.com:19302";
static const std::string kAudioLabel = "x-remote-track-audio";
static const std::string kDataChanId = "x-remote-chan-input";
static const std::string kCameraVideoLabel = "x-remote-track-camera";
static const std::string kScreenVideoLabel = "x-remote-track-screen";

using SignalingState = webrtc::PeerConnectionInterface::SignalingState;

struct SetLocalSDPCallback
    : public webrtc::SetLocalDescriptionObserverInterface {
    static rtc::scoped_refptr<SetLocalSDPCallback> Create(PeerClient *that)
    {
        return rtc::make_ref_counted<SetLocalSDPCallback>(that);
    }
    void OnSetLocalDescriptionComplete(webrtc::RTCError error) override
    {

        if (!error.ok()) {
            logger::error("failed to set local description: {}",
                          error.message());
            return;
        }

        std::string sdp;
        auto desc = that_->pc_->local_description();
        desc->ToString(&sdp);

        logger::debug("set local {}:\n{}", desc->type(), sdp);

        auto mt =
            that_->is_caller() ? MessageType::kOffer : MessageType::kAnswer;
        that_->signaling_observer_->SendSignal(mt, sdp);
    }

    PeerClient *that_;
    SetLocalSDPCallback(PeerClient *that) : that_(that) {}
};

struct SetRemoteSDPCallback
    : public webrtc::SetRemoteDescriptionObserverInterface {
    static rtc::scoped_refptr<SetRemoteSDPCallback> Create(PeerClient *that)
    {
        return rtc::make_ref_counted<SetRemoteSDPCallback>(that);
    }
    void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override
    {
        if (!error.ok()) {
            logger::error("failed to set remote description: {}",
                          error.message());
            return;
        }

        if (!that_->is_caller()) {
            that_->pc_->SetLocalDescription(SetLocalSDPCallback::Create(that_));
        }
    }

    PeerClient *that_;
    SetRemoteSDPCallback(PeerClient *that) : that_(that) {}
};

PeerClient::PeerClient(Config conf) : conf_(std::move(conf))
{
    mq_ = std::make_unique<MessageQueue>();
    signaling_thread_ = rtc::Thread::CreateWithSocketServer();
    signaling_thread_->Start();

    std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory =
        std::make_unique<CustomVideoEncoderFactory>();
    std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory =
        std::make_unique<CustomVideoDecoderFactory>();

    pc_factory_ = webrtc::CreatePeerConnectionFactory(
        nullptr, nullptr, signaling_thread_.get(), nullptr,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        std::move(encoder_factory), //
        std::move(decoder_factory), //
        nullptr, nullptr);

    webrtc::PeerConnectionFactoryInterface::Options factory_opts;
    // disable_encryption would make data_channel create failed, bug?
    /* factory_opts.disable_encryption = true; */
    pc_factory_->SetOptions(factory_opts);
}

PeerClient::~PeerClient()
{
    if (pc_)
        delete_peer_connection();
}

bool PeerClient::create_peer_connection()
{
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer server;

    server.uri = kDefaultSTUNServer;
    config.servers.push_back(server);
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.network_preference = rtc::AdapterType::ADAPTER_TYPE_WIFI;

    webrtc::PeerConnectionDependencies deps(this);
    auto err_or_pc =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(deps));

    if (!err_or_pc.ok()) {
        return false;
    }

    pc_ = err_or_pc.MoveValue();

    return true;
}

void PeerClient::delete_peer_connection()
{
    local_chan_->Close();
    pc_->Close();
    pc_ = nullptr;
    mq_ = std::make_unique<MessageQueue>();
    is_caller_ = false;

    if (camera_src_ && camera_src_->state() == VideoSource::SourceState::kLive)
        camera_src_->Stop();
    if (camera_sink_)
        camera_sink_->Stop();
    if (screen_src_ && screen_src_->state() == VideoSource::kLive)
        screen_src_->Stop();
    if (screen_sink_)
        screen_sink_->Stop();
}

void PeerClient::create_transceivers()
{
    webrtc::RtpTransceiverInit init;
    init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;

    if (conf_.enable_audio) {
        auto transceiver =
            pc_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, init);
        if (!transceiver.ok()) {
            logger::error("failed to add transceiver: {}",
                          transceiver.error().message());
        }
    }

    {
        auto transceiver =
            pc_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, init);
        if (!transceiver.ok()) {
            logger::error("failed to add transceiver: {}",
                          transceiver.error().message());
        }

        if (screen_sink_) {
            screen_sink_->Start();
        }
    }
}

// TODO: use AddTransceiver instead
void PeerClient::create_media_tracks()
{
    if (conf_.enable_audio) {
        auto audio_src =
            pc_factory_->CreateAudioSource(cricket::AudioOptions());
        auto track =
            pc_factory_->CreateAudioTrack(kAudioLabel, audio_src.get());
        auto result = pc_->AddTrack(track, {kAudioLabel});
        if (!result.ok()) {
            logger::error("failed to add audio track");
        }
    }

    if (conf_.enable_camera) {
        if (camera_src_) {
            camera_src_->Start();
            auto track = pc_factory_->CreateVideoTrack(kCameraVideoLabel,
                                                       camera_src_.get());
            auto result = pc_->AddTrack(track, {kCameraVideoLabel});
            if (!result.ok()) {
                logger::error("failed to add camera video track");
            }
        }
    }

    if (conf_.enable_screen) {
        if (screen_src_) {
            screen_src_->Start();
            // the scoped_refptr version will throw a weird `bad_alloc`, bug?
            auto track = pc_factory_->CreateVideoTrack(kScreenVideoLabel,
                                                       screen_src_.get());
            auto result = pc_->AddTrack(track, {kScreenVideoLabel});
            if (!result.ok()) {
                logger::error("failed to add screen video track");
            }
        }
    }

    if (conf_.use_codec) {
        auto capabilities =
            pc_factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO);
        logger::debug("supported codecs:");
        for (auto &codec : capabilities.codecs) {
            logger::debug("codec: kind={} mime={} name={}", codec.kind,
                          codec.mime_type(), codec.name);
        }

        auto &codecs = capabilities.codecs;
        auto cap = std::find_if(codecs.cbegin(), codecs.cend(),
                                [this](const auto &it) {
                                    return it.mime_type() == conf_.video_codec;
                                });
        if (cap == codecs.cend()) {
            return;
        }

        auto transceivers = pc_->GetTransceivers();
        auto video_sender = std::find_if(
            transceivers.cbegin(), transceivers.cend(),
            [](const auto &it) -> bool {
                return it->media_type() == cricket::MEDIA_TYPE_VIDEO &&
                       it->sender() &&
                       it->sender()->track()->id() == kScreenVideoLabel;
            });
        if (video_sender == transceivers.cend()) {
            return;
        }
        logger::debug("set remote video encoder to {}", conf_.video_codec);
        std::vector<webrtc::RtpCodecCapability> prefered_codecs = {*cap};
        video_sender->get()->SetCodecPreferences(prefered_codecs);
    }
}

void PeerClient::create_data_channel()
{
    // TODO: split chat/file transport/clipboard/drag-n-drop
    webrtc::DataChannelInit config{.protocol = "x-remote-input"};
    auto data_chan = pc_->CreateDataChannelOrError(kDataChanId, &config);
    if (data_chan.ok()) {
        local_chan_ = data_chan.value();
    } else {
        logger::error("failed to add data channel");
    }
}

void PeerClient::add_camera_video_source(VideoSourcePtr src)
{
    camera_src_ = std::move(src);
}

void PeerClient::add_screen_video_source(VideoSourcePtr src)
{
    screen_src_ = std::move(src);
}

void PeerClient::add_camera_sinks(VideoSinkPtr sink)
{
    camera_sink_ = std::move(sink);
}

void PeerClient::add_screen_sinks(VideoSinkPtr sink)
{
    screen_sink_ = std::move(sink);
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
    create_peer_connection();
    create_transceivers();
    create_data_channel();

    assert(pc_->signaling_state() ==
           webrtc::PeerConnectionInterface::SignalingState::kStable);
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
    pc_->SetLocalDescription(SetLocalSDPCallback::Create(this));
}

void PeerClient::onRemoteOffer(const std::string &sdp)
{
    logger::debug("got remote offer:\n{}", sdp);
    is_caller_ = false;

    create_peer_connection();
    create_media_tracks();
    create_data_channel();

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
    assert(is_caller());
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

// FIXME: ensure close once
void PeerClient::onRemoteBye()
{
    logger::debug("remote send a disconnect");
    delete_peer_connection();
}

// FIXME: ensure close once
void PeerClient::onDisconnect()
{
    logger::debug("disconnect from server and peer");
    delete_peer_connection();
}

// PeerConnectionObserver impl

void PeerClient::OnTrack(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
    auto track = transceiver->receiver()->track().release();
    logger::debug("OnTrack: stream_id[0]: {} receiver->id: {} track->id: {}",
                  transceiver->receiver()->stream_ids()[0],
                  transceiver->receiver()->id(), track->id());
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto video_track = static_cast<webrtc::VideoTrackInterface *>(track);
        // `track->id` is not guaranteed to be the same as `label` in remote
        // pc_factory_->CreateVideoTrack(label), use stream id instead
        if (transceiver->receiver()->stream_ids()[0] == kScreenVideoLabel &&
            screen_sink_) {
            video_track->AddOrUpdateSink(screen_sink_.get(),
                                         rtc::VideoSinkWants());
        }
        if (transceiver->receiver()->stream_ids()[0] == kCameraVideoLabel &&
            camera_sink_) {
            video_track->AddOrUpdateSink(camera_sink_.get(),
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
        if (receiver->stream_ids()[0] == kScreenVideoLabel && screen_sink_) {
            video_track->RemoveSink(screen_sink_.get());
        }
        if (receiver->stream_ids()[0] == kCameraVideoLabel && camera_sink_) {
            video_track->RemoveSink(camera_sink_.get());
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

bool PeerClient::post_text_message(const std::string &text)
{
    if (!local_chan_)
        return false;
    webrtc::DataBuffer blob(text);
    return local_chan_->Send(blob);
}

bool PeerClient::post_binary_message(const uint8_t *data, size_t size)
{
    if (!local_chan_)
        return false;
    rtc::CopyOnWriteBuffer buf(data, size);
    webrtc::DataBuffer blob{buf, true};
    return local_chan_->Send(blob);
}

std::optional<PeerClient::ChanMessage> PeerClient::poll_remote_message()
{
    if (mq_->empty())
        return {};

    ChanMessage m = mq_->front();
    mq_->pop();

    return m;
}

void PeerClient::get_stats() { pc_->GetStats(stats_observer_); }
