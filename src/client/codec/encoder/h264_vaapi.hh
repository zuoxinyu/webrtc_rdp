#pragma once
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/h264/h264_bitstream_parser.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class FFMPEGEncoder : public webrtc::VideoEncoder
{
  public:
    const char *kDeviceVAAPI = "h264_vaapi";

  public:
    FFMPEGEncoder(const webrtc::SdpVideoFormat &format);
    ~FFMPEGEncoder() override;
    int InitEncode(const webrtc::VideoCodec *codec_settings,
                   const webrtc::VideoEncoder::Settings &settings) override;

    int32_t RegisterEncodeCompleteCallback(
        webrtc::EncodedImageCallback *callback) override;

    int32_t Release() override;

    int32_t
    Encode(const webrtc::VideoFrame &frame,
           const std::vector<webrtc::VideoFrameType> *frame_types) override;

    EncoderInfo GetEncoderInfo() const override;

    void SetRates(const RateControlParameters &parameters) override;

  private:
    int intoAVFrame(AVFrame *swframe, const webrtc::VideoFrame &frame);
    int intoEncodedImage(webrtc::EncodedImage &image, const AVPacket *pkt,
                         const webrtc::VideoFrame &frame);
    int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx);

  private:
    // external resources
    webrtc::EncodedImageCallback *callback_ = nullptr;
    // internal resources
    webrtc::H264BitstreamParser h264_bit_stream_parser_;
    const AVCodec *codec_ = nullptr;
    AVCodecContext *avctx_ = nullptr;
    AVFrame *swframe_ = nullptr;
    AVFrame *hwframe_ = nullptr;
    AVPacket *packet_ = nullptr;
    AVBufferRef *device_ctx_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};
