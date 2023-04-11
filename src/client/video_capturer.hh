#pragma once

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "pc/video_track_source.h"

struct ScreenCapturer : public webrtc::VideoTrackSource {
  public:
    struct Config {
        std::vector<webrtc::WindowId> exlude_window_id;
    };

  public:
    static rtc::scoped_refptr<ScreenCapturer> Create(Config conf);

  public:
    ScreenCapturer(Config conf);
    ~ScreenCapturer() override = default;
    void setExludeWindow(webrtc::WindowId id)
    {
        conf_.exlude_window_id.emplace_back(id);
    }
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return source_.get();
    }

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
    Config conf_;
};
