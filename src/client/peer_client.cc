#include "peer_client.hh"

#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

static const std::string kDefaultSTUNServer = "stun:stun1.l.google.com:19302";
static const std::string kAudioLabel = "x-remote-track-audio";
static const std::string kDataChanId = "x-remote-chan-input";
static const std::string kCameraVideoLabel = "x-remote-track-camera";
static const std::string kScreenVideoLabel = "x-remote-track-screen";
static const int kStartBitrate = 100 * 1000 * 1000; // 100Mbps
static const int kMaxBitrate = 1000 * 1000 * 1000;  // 1000Mbps
static const int kMinBitrate = 10 * 1000 * 1000;    // 10Mbps
static const int kMaxFPS = 60;

using SignalingState = webrtc::PeerConnectionInterface::SignalingState;

static void
set_encoding_params(rtc::scoped_refptr<webrtc::RtpSenderInterface> &&sender)
{
    webrtc::RtpEncodingParameters encode_param;
    encode_param.max_bitrate_bps = kMaxBitrate;
    encode_param.min_bitrate_bps = kMinBitrate;
    encode_param.max_framerate = kMaxFPS;
    encode_param.network_priority = webrtc::Priority::kHigh;
    encode_param.bitrate_priority = 4.0; // high

    webrtc::RtpParameters params = sender->GetParameters();

    if (params.encodings.empty()) {
        params.encodings.emplace_back(encode_param);
    } else {
        for (auto &ep : params.encodings) {
            ep.max_bitrate_bps = kMaxBitrate;
            ep.min_bitrate_bps = kMinBitrate;
            ep.max_framerate = kMaxFPS;
            ep.network_priority = webrtc::Priority::kHigh;
            ep.bitrate_priority = 4.0;
        }
    }

    sender->SetParameters(params);
}

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

    pc_factory_ = webrtc::CreatePeerConnectionFactory(
        nullptr, nullptr, signaling_thread_.get(), nullptr,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        /* std::make_unique<CustomVideoEncoderFactory>(), // */
        /* std::make_unique<CustomVideoDecoderFactory>(), // */
        nullptr, nullptr);

    webrtc::PeerConnectionFactoryInterface::Options factory_opts;
    // disable_encryption would make data_channel create failed, bug?
    /* factory_opts.disable_encryption = true; */
    pc_factory_->SetOptions(factory_opts);

    {
        auto capabilities =
            pc_factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO);
        auto &codecs = capabilities.codecs;
        std::vector<std::pair<std::string, std::string>> codec_names{
            capabilities.codecs.size()};
        for (auto &codec : codecs) {
            codec_names.emplace_back(codec.mime_type(), codec.name);
        }
        logger::debug("supported codecs: {}", codec_names);
        auto findfn = [this](const auto &it) {
            return it.mime_type() == conf_.video_codec;
        };
        auto cap = std::find_if(codecs.cbegin(), codecs.cend(), findfn);
        if (cap != codecs.cend()) {
            logger::debug("prefered codecs: {}", conf_.video_codec);
            prefered_codecs_.emplace_back(*cap);
        }
    }
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
    webrtc::BitrateSettings bitrate_settings;

    bitrate_settings.start_bitrate_bps = kStartBitrate;
    bitrate_settings.max_bitrate_bps = kMaxBitrate;
    bitrate_settings.min_bitrate_bps = kMinBitrate;

    server.uri = kDefaultSTUNServer;
    config.disable_ipv6_on_wifi = true;
    config.servers.push_back(server);
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.network_preference = rtc::AdapterType::ADAPTER_TYPE_LOOPBACK;
    config.screencast_min_bitrate = kMinBitrate;
    config.audio_jitter_buffer_min_delay_ms = 0;

    webrtc::PeerConnectionDependencies deps(this);
    auto err_or_pc =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(deps));

    if (!err_or_pc.ok()) {
        return false;
    }

    pc_ = err_or_pc.MoveValue();
    pc_->SetBitrate(bitrate_settings);

    return true;
}

void PeerClient::delete_peer_connection()
{
    pc_->Close();
    pc_ = nullptr;
    mq_ = std::make_unique<MessageQueue>();
    is_caller_ = false;

    if (camera_src_ && camera_src_->state() == VideoTrackSource::kLive)
        camera_src_->Stop();
    if (screen_src_ && screen_src_->state() == VideoTrackSource::kLive)
        screen_src_->Stop();
    if (camera_sink_)
        camera_sink_->Stop(); // TODO: move to RemoveSink?
    if (screen_sink_)
        screen_sink_->Stop();
}

void PeerClient::create_transceivers()
{
    webrtc::RtpTransceiverInit init;
    init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;

    if (conf_.enable_audio) {
        auto trans =
            pc_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, init);
        if (!trans.ok()) {
            logger::error("failed to add transceiver: {}",
                          trans.error().message());
        }
    }

    if (conf_.enable_screen) {
        auto trans =
            pc_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, init);
        if (!trans.ok()) {
            logger::error("failed to add transceiver: {}",
                          trans.error().message());
        } else {
            trans.value()->receiver()->SetJitterBufferMinimumDelay(0.0);
        }
    }
}

// ~~TODO: use AddTransceiver instead~~
// see: https://groups.google.com/g/discuss-webrtc/c/wDFPFIBRRPs/m/xoPXKKC1BQAJ
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

    if (conf_.enable_camera && camera_src_) {
        camera_src_->Start();
        auto track =
            pc_factory_->CreateVideoTrack(kCameraVideoLabel, camera_src_.get());
        auto result = pc_->AddTrack(track, {kCameraVideoLabel});
        if (!result.ok()) {
            logger::error("failed to add camera video track");
        } else {
            set_encoding_params(result.MoveValue());
        }
    }

    if (conf_.enable_screen && screen_src_) {
        screen_src_->Start();
        // the scoped_refptr version will throw a weird `bad_alloc`, bug?
        auto track =
            pc_factory_->CreateVideoTrack(kScreenVideoLabel, screen_src_.get());
        auto result = pc_->AddTrack(track, {kScreenVideoLabel});
        if (!result.ok()) {
            logger::error("failed to add screen video track");
        } else {
            set_encoding_params(result.MoveValue());
        }
    }

    if (conf_.use_codec && !prefered_codecs_.empty()) {
        auto ts = pc_->GetTransceivers();
        for (const auto &t : ts) {
            if (t->media_type() != cricket::MediaType::MEDIA_TYPE_VIDEO) {
                continue;
            }
            auto result = t->SetCodecPreferences(prefered_codecs_);
            if (!result.ok()) {
                logger::error("failed to set preferred codecs");
            }
        }
    }
}

void PeerClient::create_data_channel()
{
    // TODO: split chat/file transport/clipboard/drag-n-drop
    webrtc::DataChannelInit config{.protocol = "x-remote-input"};
    auto data_chan = pc_->CreateDataChannelOrError(kDataChanId, &config);
    if (data_chan.ok()) {
        data_chan_ = data_chan.value();
        data_chan_->RegisterObserver(this);
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
    // only caller need to create data chan
    create_data_channel();

    assert(pc_->signaling_state() ==
           webrtc::PeerConnectionInterface::SignalingState::kStable);
    // TODO: modify SDP for `b=AS:[bitrate]`
    pc_->SetLocalDescription(SetLocalSDPCallback::Create(this));
}

void PeerClient::onRemoteOffer(const std::string &sdp)
{
    logger::debug("got remote offer:\n{}", sdp);
    is_caller_ = false;

    create_peer_connection();
    create_media_tracks();

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
    // logger::debug("got remote candidate:\n{}", sdp);
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
    logger::debug("OnTrack: stream_id[0]: {} track->id: {}",
                  transceiver->receiver()->stream_ids()[0], track->id());
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto video_track = static_cast<webrtc::VideoTrackInterface *>(track);
        // `track->id` is not guaranteed to be the same as `label` in remote
        // pc_factory_->CreateVideoTrack(`label`), use stream id instead
        if (transceiver->receiver()->stream_ids()[0] == kScreenVideoLabel &&
            screen_sink_) {
            video_track->AddOrUpdateSink(screen_sink_.get(),
                                         rtc::VideoSinkWants());
            screen_sink_->Start();
        }
        if (transceiver->receiver()->stream_ids()[0] == kCameraVideoLabel &&
            camera_sink_) {
            video_track->AddOrUpdateSink(camera_sink_.get(),
                                         rtc::VideoSinkWants());
            camera_sink_->Start();
        }
    } else if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
        auto audio_track = static_cast<webrtc::AudioTrackInterface *>(track);
        if (transceiver->receiver()->stream_ids()[0] == kAudioLabel) {
            // do nothing for now
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
    data_chan_ = data_channel;
    data_chan_->RegisterObserver(this);
}

void PeerClient::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
    if (!candidate) {
        logger::error("OnIceCandidate got null candidate");
        return;
    }
    std::string candi;
    candidate->ToString(&candi);

    // logger::debug("got local candidate:\n{}", candi);

    signaling_observer_->SendSignal(MessageType::kCandidate, candi);
}

void PeerClient::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
}

void PeerClient::OnIceSelectedCandidatePairChanged(
    const cricket::CandidatePairChangeEvent &event)
{
    logger::debug("ICE selected candidate changed, reason: {}\n"
                  "  local : {}\n"
                  "  remote: {}\n",
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
        // logger::debug("recv remote text message: {}", m);
    }
    mq_->push(PeerClient::ChanMessage{msg.data.data(), msg.size(), msg.binary});
}

bool PeerClient::post_text_message(const std::string &text)
{
    if (!data_chan_)
        return false;
    webrtc::DataBuffer blob(text);
    return data_chan_->Send(blob);
}

bool PeerClient::post_binary_message(const uint8_t *data, size_t size)
{
    if (!data_chan_)
        return false;
    rtc::CopyOnWriteBuffer buf(data, size);
    webrtc::DataBuffer blob{buf, true};
    return data_chan_->Send(blob);
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
