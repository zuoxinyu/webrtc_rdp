#pragma once

#include <memory>

#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"

struct PeerConnectionImpl : public webrtc::PeerConnectionObserver,
                            public webrtc::CreateSessionDescriptionObserver {
  public:
    PeerConnectionImpl();
    ~PeerConnectionImpl() = default;

    bool createPeerConnection();
    void deletePeerConnection();
    void addTracks();
    void addSinks(rtc::VideoSinkInterface<webrtc::VideoFrame> *);

  public: // PeerConnectionObserver impl
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override;
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

  public: // CreateSessionDescriptionObserver impl
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
    void OnFailure(webrtc::RTCError error) override;

  private:
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
};
