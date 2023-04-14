#pragma once

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "pc/video_track_source.h"

struct FakeCapturer : public webrtc::VideoTrackSource {
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

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
};
