#pragma once

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "pc/video_track_source.h"

struct CameraCapturer : public webrtc::VideoTrackSource {
  public:
    struct Config {
        int width;
        int height;
        int fps;
    };

  public:
    CameraCapturer(Config conf);
    ~CameraCapturer() override = default;

    static size_t GetDeviceNum();
    static rtc::scoped_refptr<CameraCapturer> Create(Config conf);

  public:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return source_.get();
    }

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
};
