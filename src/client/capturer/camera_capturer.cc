#include "camera_capturer.hh"
#include "logger.hh"

#include "api/video/video_source_interface.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"

class CameraCapturerImpl : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
  public:
    CameraCapturerImpl(const CameraCapturer::Config &conf)
    {
        auto device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
        char device_name[256];
        char device_uniq[256];
        auto ndev = device_info->NumberOfDevices();
        logger::debug("device number: {}", ndev);
        for (auto i = 0; i < ndev; i++) {
            device_info->GetDeviceName(i, device_name, 256, device_uniq, 256);
            logger::debug("device[{}]: {}, {}", i, device_name, device_uniq);
        }
        assert(ndev > 0);
        device_info->GetDeviceName(0, device_name, 256, device_uniq, 256);
        vcm_ = webrtc::VideoCaptureFactory::Create(device_uniq);

        start();
    }

  public: // impl VideoSourceInterface
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                         const rtc::VideoSinkWants &wants) override
    {
        vcm_->RegisterCaptureDataCallback(sink);
    };

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        vcm_->DeRegisterCaptureDataCallback();
    };

    void RequestRefreshFrame() override{};

    bool running() const { return running_; }
    void start()
    {
        webrtc::VideoCaptureCapability cap;
        cap.width = conf_.width;
        cap.height = conf_.height;
        cap.videoType = webrtc::VideoType::kI420;
        cap.maxFPS = conf_.fps;
        vcm_->StartCapture(cap);
    }

  private:
    rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
    bool running_ = false;
    CameraCapturer::Config conf_;
};

rtc::scoped_refptr<CameraCapturer> CameraCapturer::Create(Config conf)
{
    return rtc::make_ref_counted<CameraCapturer>(conf);
}

size_t CameraCapturer::GetDeviceNum()
{
    auto device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    return device_info->NumberOfDevices();
}

CameraCapturer::CameraCapturer(CameraCapturer::Config conf)
    : webrtc::VideoTrackSource(false)
{
    source_ = std::make_unique<CameraCapturerImpl>(conf);
}
