#include "video_capturer.hh"
#include "logger.hh"

#include <algorithm>
#include <cstdlib>
#include <thread>
#include <vector>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/time_utils.h"

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"

#include <libyuv/convert.h>
#include <libyuv/video_common.h>
class ScreenCaptureImpl : public rtc::VideoSourceInterface<webrtc::VideoFrame>,
                          public webrtc::DesktopCapturer::Callback
{
  public:
    enum CaptureType {
        kWindow = 0,
        kScreen = 1,
    };

  public:
    ScreenCaptureImpl(const ScreenCapturer::Config &conf,
                      CaptureType kind = CaptureType::kScreen)
        : desktop_capturer_(nullptr), thread_(), sinks_()
    {
        auto opts = webrtc::DesktopCaptureOptions::CreateDefault();
#ifdef __unix__
        std::string display = std::getenv("DISPLAY");
        logger::debug("display: {}", display);
        auto xdisplay = webrtc::SharedXDisplay::CreateDefault();
        opts.set_x_display(xdisplay);
#endif

        if (kind == CaptureType::kScreen) {
            desktop_capturer_ =
                webrtc::DesktopCapturer::CreateScreenCapturer(opts);
        } else {
            desktop_capturer_ =
                webrtc::DesktopCapturer::CreateWindowCapturer(opts);
        }

        webrtc::DesktopCapturer::SourceList sources;
        desktop_capturer_->GetSourceList(&sources);
        logger::debug("capture sources: ");
        for (auto &src : sources) {
            logger::debug("source: {}", src.title);
        }

        for (auto id : conf.exlude_window_id) {
            desktop_capturer_->SetExcludedWindow(id);
        }
        // Start should only be called once
        desktop_capturer_->Start(this);
    }

    ~ScreenCaptureImpl() override
    {
        if (running_)
            stop();
    }

    bool running() const { return running_; }

    void start()
    {
        assert(!running_);
        running_ = true;
        thread_ = std::thread(&ScreenCaptureImpl::capture_thread, this);
    }

    void stop()
    {
        assert(running_);
        running_ = false;
        thread_.join();
    }

    void capture_thread()
    {
        logger::debug("start capture thread");
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
            sinks_.emplace_back(sink, wants);
            logger::debug("new sink added: {}", sinks_.size());
        } else {
            pair->wants = wants;
        }
    };

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        std::find_if(
            sinks_.begin(), sinks_.end(),
            [sink](const SinkPair &pair) { return pair.sink == sink; });
        logger::debug("sink removed: {}", sinks_.size());
    };

    void RequestRefreshFrame() override{};

  public: // impl DesktopCapturer::Callback
    void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                         std::unique_ptr<webrtc::DesktopFrame> frame) override
    {
        static int16_t id = 0;
        if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
            return;
        }

        auto buffer = webrtc::I420Buffer::Create(frame->size().width(),
                                                 frame->size().height());

        libyuv::ConvertToI420(
            frame->data(), 0, buffer->MutableDataY(), buffer->StrideY(),
            buffer->MutableDataU(), buffer->StrideU(), buffer->MutableDataV(),
            buffer->StrideV(), 0, 0, frame->size().width(),
            frame->size().height(), buffer->width(), buffer->height(),
            libyuv::kRotate0, libyuv::FOURCC_ARGB);

        webrtc::VideoFrame::Builder builder;
        auto captured_frame = builder.set_rotation(webrtc::kVideoRotation_0)
                                  .set_id(id++)
                                  .set_timestamp_us(rtc::TimeMicros())
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
    std::atomic<bool> running_ = false;
};

rtc::scoped_refptr<ScreenCapturer> ScreenCapturer::Create(Config conf)
{
    return rtc::make_ref_counted<ScreenCapturer>(conf);
}

ScreenCapturer::ScreenCapturer(Config conf)
    : VideoSource(false), conf_(std::move(conf))
{
    source_ =
        std::make_unique<ScreenCaptureImpl>(conf_, ScreenCaptureImpl::kScreen);
    logger::debug("ScreenCapturer created");
}

ScreenCapturer::~ScreenCapturer() = default;

void ScreenCapturer::Start()
{
    SetState(SourceState::kLive);
    static_cast<ScreenCaptureImpl *>(source_.get())->start();
}

void ScreenCapturer::Stop()
{
    SetState(SourceState::kEnded);
    static_cast<ScreenCaptureImpl *>(source_.get())->stop();
}
