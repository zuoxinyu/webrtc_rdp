#include "fake_capturer.hh"
#include "logger.hh"

#include "api/video/video_source_interface.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"

extern "C" {
#include <third_party/ffmpeg/libavcodec/avcodec.h>
#include <third_party/ffmpeg/libavcodec/codec.h>
#include <third_party/ffmpeg/libavcodec/packet.h>
#include <third_party/ffmpeg/libavformat/avformat.h>
#include <third_party/ffmpeg/libavutil/frame.h>
}

class FakeCapturerImpl : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
  public:
    FakeCapturerImpl(const FakeCapturer::Config &conf)
    {
        int ret;
        fmtctx_ = avformat_alloc_context();
        assert(fmtctx_);
        ret = avformat_open_input(&fmtctx_, url_.c_str(), nullptr, nullptr);
        if (ret) {
        }
        codec_ctx_ = avcodec_alloc_context3(nullptr);
        assert(codec_ctx_);
        ret = avcodec_parameters_to_context(codec_ctx_,
                                            fmtctx_->streams[0]->codecpar);
        if (ret) {
        }

        codec_ = avcodec_find_decoder(codec_ctx_->codec_id);
        ret = avcodec_open2(codec_ctx_, codec_, nullptr);
        if (ret) {
        }

        pkt_ = av_packet_alloc();
        frame_ = av_frame_alloc();
    }

  public: // impl VideoSourceInterface
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                         const rtc::VideoSinkWants &wants) override{};

    void
    RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override{};

    void RequestRefreshFrame() override{};

    bool running() const { return running_; }
    void decoder_thread()
    {
        int ret;
        while (true) {
            ret = av_read_frame(fmtctx_, pkt_);
            switch (ret) {
            }

            ret = avcodec_send_packet(codec_ctx_, pkt_);
            switch (ret) {
            }

            while (true) {
                ret = avcodec_receive_frame(codec_ctx_, frame_);
                switch (ret) {
                }
                // translate_frame
                // OnFrame
            }
        }
    }
    void translate_frame(AVFrame *f, webrtc::VideoFrame *vf) {}

  private:
    // internal resources
    AVFormatContext *fmtctx_;
    AVCodecContext *codec_ctx_;
    const AVCodec *codec_;
    AVPacket *pkt_;
    AVFrame *frame_;

    // external resources
    std::string url_;
    FakeCapturer::Config conf_;
    // states
    bool running_ = false;
};

rtc::scoped_refptr<FakeCapturer> FakeCapturer::Create(Config conf)
{
    return rtc::make_ref_counted<FakeCapturer>(conf);
}

size_t FakeCapturer::GetDeviceNum()
{
    auto device_info = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    return device_info->NumberOfDevices();
}

FakeCapturer::FakeCapturer(FakeCapturer::Config conf)
    : webrtc::VideoTrackSource(false)
{
    source_ = std::make_unique<FakeCapturerImpl>(conf);
}
