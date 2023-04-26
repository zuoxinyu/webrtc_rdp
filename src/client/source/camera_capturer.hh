#pragma once

#include "video_source.hh"

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"

struct CameraCapturer : public VideoSource {
  public:
    struct Config {
        int width;
        int height;
        int fps;
        const char *uniq;
    };
    using DeviceList = std::vector<std::pair<std::string, std::string>>;

  public:
    CameraCapturer(Config conf);
    ~CameraCapturer() override = default;

    static DeviceList GetDeviceList();
    static rtc::scoped_refptr<CameraCapturer> Create(Config conf);

  public:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return source_.get();
    }

    void Start() override;
    void Stop() override;

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
};
