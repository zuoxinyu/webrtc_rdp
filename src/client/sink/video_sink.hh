#pragma once

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

struct VideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
                   public rtc::RefCountInterface {

    virtual void Start() = 0;
    virtual void Stop() = 0;

    ~VideoSink() override = default;
};
