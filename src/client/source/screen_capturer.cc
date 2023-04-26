#include "screen_capturer.hh"
#include "logger.hh"

#ifdef __linux__
#include <sys/prctl.h>
#endif

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
        : conf_(conf)
    {
        auto opts = webrtc::DesktopCaptureOptions::CreateDefault();
#ifdef __linux__
        // opts.set_full_screen_window_detector(nullptr);
        std::string display = std::getenv("DISPLAY");
        logger::debug("display: {}", display);
        auto xdisplay = webrtc::SharedXDisplay::CreateDefault();
        opts.set_x_display(xdisplay);
        // opts.set_prefer_cursor_embedded(true);
        // opts.set_detect_updated_region(true);
#endif

        if (kind == CaptureType::kScreen) {
            desktop_capturer_ =
                webrtc::DesktopCapturer::CreateScreenCapturer(opts);
        } else {
            desktop_capturer_ =
                webrtc::DesktopCapturer::CreateWindowCapturer(opts);
        }

        desktop_capturer_->SetMaxFrameRate(conf_.fps);
        {
            webrtc::DesktopCapturer::SourceList sources;
            std::vector<std::string> names;
            desktop_capturer_->GetSourceList(&sources);
            std::transform(sources.begin(), sources.end(),
                           std::back_insert_iterator(names),
                           [](auto it) { return it.title; });
            logger::debug("capture sources: {}", names);
        }

        for (auto id : conf.exlude_window_id) {
            desktop_capturer_->SetExcludedWindow(id);
        }

        scaled_buffer_ = webrtc::I420Buffer::Create(conf_.width, conf_.height);
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
        prctl(PR_SET_NAME, reinterpret_cast<unsigned long>("screen_capture"));
        logger::debug("start capture thread");
        while (running()) {
            desktop_capturer_->CaptureFrame();
            // TODO: use timer instead?
            // std::this_thread::sleep_for(std::chrono::milliseconds(16));
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
        } else {
            pair->wants = wants;
        }
    };

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        std::erase_if(
            sinks_, [sink](const SinkPair &pair) { return pair.sink == sink; });
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

        if (!origin_buffer_)
            origin_buffer_ = webrtc::I420Buffer::Create(frame->size().width(),
                                                        frame->size().height());

        libyuv::ConvertToI420(
            frame->data(), 0,                                          //
            origin_buffer_->MutableDataY(), origin_buffer_->StrideY(), //
            origin_buffer_->MutableDataU(), origin_buffer_->StrideU(), //
            origin_buffer_->MutableDataV(), origin_buffer_->StrideV(), //
            0, 0,                                                      //
            frame->size().width(), frame->size().height(),             //
            frame->size().width(), frame->size().height(),             //
            libyuv::kRotate0, libyuv::FOURCC_ARGB);

        libyuv::I420Scale(
            origin_buffer_->DataY(), origin_buffer_->StrideY(),        //
            origin_buffer_->DataU(), origin_buffer_->StrideU(),        //
            origin_buffer_->DataV(), origin_buffer_->StrideV(),        //
            origin_buffer_->width(), origin_buffer_->height(),         //
            scaled_buffer_->MutableDataY(), scaled_buffer_->StrideY(), //
            scaled_buffer_->MutableDataU(), scaled_buffer_->StrideU(), //
            scaled_buffer_->MutableDataV(), scaled_buffer_->StrideV(), //
            scaled_buffer_->width(), scaled_buffer_->height(),         //
            libyuv::kFilterBox);

        webrtc::VideoFrame::Builder builder;
        auto captured_frame = builder.set_rotation(webrtc::kVideoRotation_0)
                                  .set_id(id++)
                                  .set_timestamp_us(rtc::TimeMicros())
                                  .set_video_frame_buffer(scaled_buffer_)
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
    rtc::scoped_refptr<webrtc::I420Buffer> origin_buffer_;
    rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer_;
    ScreenCapturer::Config conf_;
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
