#pragma once

#include "video_source.hh"

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"

struct FakeCapturer : public VideoSource {
  public:
    struct Config {
        int width;
        int height;
        int fps;
    };

  public:
    FakeCapturer(Config conf);
    ~FakeCapturer() override = default;

    static size_t GetDeviceNum();
    static rtc::scoped_refptr<FakeCapturer> Create(Config conf);

  public:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return source_.get();
    }

    void Start() override {}

    void Stop() override {}

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
};
