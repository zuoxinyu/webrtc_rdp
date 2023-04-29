#pragma once

#include "video_source.hh"

#include <memory>

#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "modules/desktop_capture/desktop_capture_types.h"

struct ScreenCapturer : public VideoTrackSource {
  public:
    struct Config {
        int fps = 60;
        int width = 0;
        int height = 0;
        bool keep_ratio = true;
        std::vector<webrtc::WindowId> exlude_window_id;
    };

  public:
    ScreenCapturer(Config conf);
    ~ScreenCapturer() override;

    static rtc::scoped_refptr<ScreenCapturer> Create(Config conf);

    void set_exlude_window(webrtc::WindowId id)
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
