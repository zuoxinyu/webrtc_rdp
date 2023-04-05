#include "video_capturer.hh"

#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video/yuv_helper.h"

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"

// TODO: impl VideoFrame
class ScreenCaptureImpl : public rtc::VideoSourceInterface<webrtc::VideoFrame>,
                          public webrtc::DesktopCapturer::Callback
{
  public:
    enum CaptureType {
        kWindow = 0,
        kScreen = 1,
    };

  public:
    ScreenCaptureImpl(CaptureType kind = CaptureType::kScreen)
        : desktop_capturer_(nullptr), thread_(), sinks_()
    {
        std::cout << "ScreenCaptureImpl:" << std::endl;
        auto opts = webrtc::DesktopCaptureOptions::CreateDefault();

        if (kind == CaptureType::kScreen) {
            desktop_capturer_ =
                webrtc::DesktopCapturer::CreateScreenCapturer(opts);
        } else {
            desktop_capturer_ =
                webrtc::DesktopCapturer::CreateWindowCapturer(opts);
        }

        webrtc::DesktopCapturer::SourceList sources;
        desktop_capturer_->GetSourceList(&sources);
        std::cout << "capture sources: " << std::endl;
        for (auto &src : sources) {
            std::cout << src.title << std::endl;
        }

        start();
        std::cout << "desktop_capturer_ created" << std::endl;
    }

    virtual ~ScreenCaptureImpl() override{};

    bool running() const { return running_; }

    void start()
    {
        // TODO: create dedicate thread
        running_ = true;
        thread_ = std::thread(&ScreenCaptureImpl::capture_thread, this);
        desktop_capturer_->Start(this);
    }

    void stop()
    {
        running_ = false;
        thread_.join();
    }

    void capture_thread()
    {
        while (running()) {
            desktop_capturer_->CaptureFrame();
        }
    }

  public: // impl VideoSourceInterface
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                         const rtc::VideoSinkWants &wants) override
    {
        auto pair = std::find_if(
            sinks_.begin(), sinks_.end(),
            [sink](const SinkPair &pair) { return pair.sink == sink; });
        if (pair == sinks_.end()) {
            sinks_.push_back(SinkPair(sink, wants));
        } else {
            pair->wants = wants;
        }
    };

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        sinks_.erase(std::remove_if(
            sinks_.begin(), sinks_.end(),
            [sink](const SinkPair &pair) { return pair.sink == sink; }));
    };

    void RequestRefreshFrame() override { desktop_capturer_->CaptureFrame(); };

  public: // impl DesktopCapturer::Callback
    void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                         std::unique_ptr<webrtc::DesktopFrame> frame) override
    {
        std::cout << "OnCaptureResult: " << std::endl;
        if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
            return;
        }

        auto buffer = webrtc::I420Buffer::Create(frame->size().width(),
                                                 frame->size().height());
        // DesktopFrame is always BGRA
        webrtc::ARGBToI420(frame->data(), frame->stride(),
                           buffer->MutableDataY(), buffer->StrideY(),
                           buffer->MutableDataU(), buffer->StrideU(),
                           buffer->MutableDataV(), buffer->StrideV(),
                           frame->size().width(), frame->size().height());

        webrtc::VideoFrame::Builder builder;
        auto captured_frame = builder.set_rotation(webrtc::kVideoRotation_0)
                                  .set_timestamp_ms(frame->capture_time_ms())
                                  .set_video_frame_buffer(buffer)
                                  .build();

        // send to sinks
        std::for_each(sinks_.begin(), sinks_.end(),
                      [&captured_frame](const SinkPair &pair) {
                          pair.sink->OnFrame(captured_frame);
                      });
    }

  protected:
    struct SinkPair {
        SinkPair(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                 const rtc::VideoSinkWants &wants)
            : sink(sink), wants(wants)
        {
        }
        rtc::VideoSinkInterface<webrtc::VideoFrame> *sink;
        rtc::VideoSinkWants wants;
    };

  private:
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;
    std::thread thread_;
    std::vector<SinkPair> sinks_;
    bool running_ = false;
};

rtc::scoped_refptr<ScreenCapturer> ScreenCapturer::Create()
{
    return rtc::make_ref_counted<ScreenCapturer>();
}

ScreenCapturer::ScreenCapturer()
    : webrtc::VideoTrackSource(false),
      source_(std::make_unique<ScreenCaptureImpl>())
{
    std::cout << "ScreenCapturer created" << std::endl;
}
