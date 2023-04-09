#pragma once

#include "callbacks.hh"

#include <memory>

#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"

struct PeerClient : public webrtc::PeerConnectionObserver,
                    public webrtc::CreateSessionDescriptionObserver,
                    public PeerObserver {

  public:
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
    // UIObserver lifetime?
    PeerClient();
    ~PeerClient() = default;

    bool createPeerConnection();
    void deletePeerConnection();
    void createLocalTracks();
    void makeCall();
    void addLocalSinks(rtc::VideoSinkInterface<webrtc::VideoFrame> *);
    void addRemoteSinks(rtc::VideoSinkInterface<webrtc::VideoFrame> *);
    void onRemoteOffer(const std::string &);
    void onRemoteAnswer(const std::string &);
    void onRemoteCandidate(const std::string &);
    void setSignalingObserver(SignalingObserver *ob)
    {
        signaling_observer_ = ob;
    }
    bool isCaller() const { return is_caller_; }
    std::string offer() const { return offer_; }
    std::string answer() const { return answer_; }
    std::string candi() const { return candi_; }

  public:
    void OnMessage(MessageType, const std::string &offer) override;

  public:
    // PeerConnectionObserver impl
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

  public:
    // CreateSessionDescriptionObserver impl
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
    void OnFailure(webrtc::RTCError error) override;

  private:
    // resources
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_ =
        nullptr;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_ = nullptr;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_ = nullptr;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *remote_sink_ = nullptr;
    std::unique_ptr<rtc::Thread> signaling_thread_ = nullptr;
    SignalingObserver *signaling_observer_ = nullptr;

    // states
    bool is_caller_;
    std::string offer_;
    std::string candi_;
    std::string answer_;
};
