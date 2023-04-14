#pragma once

#include "callbacks.hh"

#include <memory>

#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "rtc_base/ref_counted_object.h"

struct PeerClient : public webrtc::PeerConnectionObserver,
                    public webrtc::CreateSessionDescriptionObserver,
                    public webrtc::DataChannelObserver,
                    public PeerObserver {

  public:
    struct ChanMessage;
    using VideoSourcePtr = webrtc::VideoTrackSourceInterface *;
    using VideoSinkPtr = rtc::VideoSinkInterface<webrtc::VideoFrame> *;
    using MessageQueue = std::queue<ChanMessage>;

    struct ChanMessage {
        const uint8_t *data;
        size_t size;
        bool binary;
    };

    struct SetRemoteSDPCallback
        : public webrtc::SetRemoteDescriptionObserverInterface {
        static rtc::scoped_refptr<SetRemoteSDPCallback> Create(PeerClient *that)
        {
            return rtc::make_ref_counted<SetRemoteSDPCallback>(that);
        }
        void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;

        PeerClient *that_;
        SetRemoteSDPCallback(PeerClient *that) : that_(that) {}
    };

    struct SetLocalSDPCallback
        : public webrtc::SetLocalDescriptionObserverInterface {
        static rtc::scoped_refptr<SetLocalSDPCallback> Create(PeerClient *that)
        {
            return rtc::make_ref_counted<SetLocalSDPCallback>(that);
        }
        void OnSetLocalDescriptionComplete(webrtc::RTCError error) override;

        PeerClient *that_;
        SetLocalSDPCallback(PeerClient *that) : that_(that) {}
    };
    friend struct SetRemoteSDPCallback;
    friend struct SetLocalSDPCallback;

  public:
    PeerClient();
    ~PeerClient() override = default;

    bool createPeerConnection();
    void deletePeerConnection();
    void createLocalTracks();
    void makeCall();
    void addLocalVideoSource(VideoSourcePtr);
    void addRemoteVideoSource(VideoSourcePtr);
    void addLocalSinks(VideoSinkPtr);
    void addRemoteSinks(VideoSinkPtr);
    void setSignalingObserver(SignalingObserver *ob)
    {
        signaling_observer_ = ob;
    }
    bool isCaller() const { return is_caller_; }

    // channel messaging, TODO: abastract interface
    bool sendTextMessage(const std::string &text);
    bool sendBinaryMessage(const uint8_t *data, size_t size);
    std::optional<ChanMessage> recvMessage();

  private:
    void onRemoteOffer(const std::string &);
    void onRemoteAnswer(const std::string &);
    void onRemoteCandidate(const std::string &);

  public: // PeerObserver impl
    void OnSignal(MessageType, const std::string &offer) override;

  public: // PeerConnectionObserver impl
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override{};
    void
    OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
            &streams) override;
    void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
    void OnAddStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
    void OnRemoveStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void OnIceSelectedCandidatePairChanged(
        const cricket::CandidatePairChangeEvent &) override;

  public: // CreateSessionDescriptionObserver impl
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
    void OnFailure(webrtc::RTCError error) override;

  public: // DataChannelInterface impl
    void OnStateChange() override {}
    void OnMessage(const webrtc::DataBuffer &) override;
    void OnBufferedAmountChange(uint64_t) override {}

  private:
    // external resources
    VideoSourcePtr local_video_src_ = nullptr;
    VideoSourcePtr remote_video_src_ = nullptr;
    VideoSinkPtr local_sink_ = nullptr;
    VideoSinkPtr remote_sink_ = nullptr;
    SignalingObserver *signaling_observer_ = nullptr;
    // internal resources
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_ =
        nullptr;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_ = nullptr;
    rtc::scoped_refptr<webrtc::DataChannelInterface> local_chan_ = nullptr;
    rtc::scoped_refptr<webrtc::DataChannelInterface> remote_chan_ = nullptr;
    std::unique_ptr<rtc::Thread> signaling_thread_ = nullptr;

    std::unique_ptr<MessageQueue> mq_;

    // states
    // TODO: perfect negotiation (e.g. use `polite peer` strategy)
    bool is_caller_;
};
