#pragma once

#include "callbacks.hh"
#include "sink/video_sink.hh"
#include "source/video_source.hh"
#include "stats/stats.hh"

#include <memory>

#include "api/peer_connection_interface.h"

struct PeerClient : private webrtc::PeerConnectionObserver,
                    private webrtc::DataChannelObserver,
                    public PeerObserver {

  public:
    struct Config {
        Config() { ; }
        bool use_codec = true;
        std::string video_codec = "video/H264";
        bool enable_chat = true;
        bool enable_audio = false;
        bool enable_screen = true;
        bool enable_camera = false;
        bool enable_control = true;
        bool enable_clipboard = false;
        bool enable_file_transfer = false;
    };

    struct ChanMessage {
        const uint8_t *data;
        size_t size;
        bool binary;
    };

    using VideoSourcePtr = rtc::scoped_refptr<VideoTrackSource>;
    using VideoSinkPtr = rtc::scoped_refptr<VideoSink>;
    using MessageQueue = std::queue<ChanMessage>;

    friend struct SetRemoteSDPCallback;
    friend struct SetLocalSDPCallback;

  public:
    PeerClient(Config conf = Config());
    ~PeerClient() override;

    // external resources about
    void add_camera_video_source(VideoSourcePtr);
    void add_screen_video_source(VideoSourcePtr);
    void add_camera_sinks(VideoSinkPtr);
    void add_screen_sinks(VideoSinkPtr);
    void set_signaling_observer(SignalingObserver *ob)
    {
        signaling_observer_ = ob;
    }
    void set_stats_observer(StatsObserver *ob) { stats_observer_ = ob; }
    // states
    bool is_caller() const { return is_caller_; }
    // channel messaging, TODO: abastract interface
    bool post_text_message(const std::string &text);
    bool post_binary_message(const uint8_t *data, size_t size);
    std::optional<ChanMessage> poll_remote_message();
    // stats
    void get_stats();

  private:
    // internal resources about
    bool create_peer_connection();
    void delete_peer_connection();
    void create_transceivers();
    void create_media_tracks();
    void create_data_channel();

    // signaling about
    void onReady();
    void onDisconnect();
    void onRemoteOffer(const std::string &);
    void onRemoteAnswer(const std::string &);
    void onRemoteCandidate(const std::string &);
    void onRemoteBye();

  public: // PeerObserver impl
    void OnSignal(MessageType, const std::string &offer) override;

  private: // PeerConnectionObserver impl
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState) override{};
    void OnIceCandidate(const webrtc::IceCandidateInterface *) override;
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>) override;
    void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface>) override;
    void OnAddStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface>) override{};
    void OnRemoveStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface>) override{};
    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface>) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState) override;
    void OnIceSelectedCandidatePairChanged(
        const cricket::CandidatePairChangeEvent &) override;

  private: // DataChannelInterface impl
    void OnStateChange() override {}
    void OnMessage(const webrtc::DataBuffer &) override;
    void OnBufferedAmountChange(uint64_t) override {}

  private:
    // external resources
    VideoSourcePtr camera_src_ = nullptr;
    VideoSourcePtr screen_src_ = nullptr;
    VideoSinkPtr camera_sink_ = nullptr;
    VideoSinkPtr screen_sink_ = nullptr;
    SignalingObserver *signaling_observer_ = nullptr;
    StatsObserver *stats_observer_ = nullptr;
    // internal resources
    // must before `pc_factory_`, due to destruction order
    std::unique_ptr<rtc::Thread> signaling_thread_ = nullptr;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_ =
        nullptr;
    // TODO: multiple pc instances support?
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_ = nullptr;
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_chan_ = nullptr;
    std::unique_ptr<MessageQueue> mq_;

    // states
    // TODO: perfect negotiation (e.g. use `polite peer` strategy)
    bool is_caller_;
    Config conf_;
    std::vector<webrtc::RtpCodecCapability> prefered_codecs_;
};
