#include "camera_capturer.hh"

#include "api/video/video_source_interface.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"

class CameraCapturerImpl : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
  public:
    CameraCapturerImpl(const CameraCapturer::Config &conf)
    {
        vcm_ = webrtc::VideoCaptureFactory::Create(conf.uniq);
    }
    ~CameraCapturerImpl() override = default;

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

    void stop() { vcm_->StopCapture(); }

  private:
    rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
    bool running_ = false;
    CameraCapturer::Config conf_;
};

rtc::scoped_refptr<CameraCapturer> CameraCapturer::Create(Config conf)
{
    return rtc::make_ref_counted<CameraCapturer>(conf);
}

CameraCapturer::CameraCapturer(CameraCapturer::Config conf)
    : VideoTrackSource(false)
{
    source_ = std::make_unique<CameraCapturerImpl>(conf);
}

void CameraCapturer::Start()
{
    SetState(SourceState::kLive);
    static_cast<CameraCapturerImpl *>(source_.get())->start();
}

void CameraCapturer::Stop()
{
    SetState(SourceState::kEnded);
    static_cast<CameraCapturerImpl *>(source_.get())->stop();
}

CameraCapturer::DeviceList CameraCapturer::GetDeviceList()
{
    auto device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    char device_name[256];
    char device_uniq[256];
    auto ndev = device_info->NumberOfDevices();
    std::vector<std::pair<std::string, std::string>> devices;
    for (auto i = 0; i < ndev; i++) {
        device_info->GetDeviceName(i, device_name, 256, device_uniq, 256);
        devices.emplace_back(device_name, device_uniq);
    }
    return devices;
}
