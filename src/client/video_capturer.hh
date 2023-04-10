#pragma once

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "pc/video_track_source.h"

struct ScreenCapturer : public webrtc::VideoTrackSource {
  public:
    static rtc::scoped_refptr<webrtc::VideoTrackSource> Create();

  public:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return source_.get();
    }

  public:
    ScreenCapturer();
    ~ScreenCapturer() override = default;

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
};
