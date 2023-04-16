#pragma once

#include "video_source.hh"

#include <memory>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "modules/desktop_capture/desktop_capture_types.h"

struct ScreenCapturer : public VideoSource {
  public:
    struct Config {
        std::vector<webrtc::WindowId> exlude_window_id;
    };

  public:
    ScreenCapturer(Config conf);
    ~ScreenCapturer() override;

    static rtc::scoped_refptr<ScreenCapturer> Create(Config conf);

    void setExludeWindow(webrtc::WindowId id)
    {
        conf_.exlude_window_id.emplace_back(id);
    }

  public:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override
    {
        return source_.get();
    }

    void Start() override;
    void Stop() override;

  private:
    std::unique_ptr<rtc::VideoSourceInterface<webrtc::VideoFrame>> source_;
    Config conf_;
};
