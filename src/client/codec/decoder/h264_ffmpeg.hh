#pragma once
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "common_video/h264/h264_bitstream_parser.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class FFMPEGDecoder : public webrtc::VideoDecoder
{
  public:
    const char *kDeviceVAAPI = "h264_vaapi";

  public:
    FFMPEGDecoder(const webrtc::SdpVideoFormat &format);
    ~FFMPEGDecoder() override;
    bool Configure(const webrtc::VideoDecoder::Settings &settings) override;

    int32_t RegisterDecodeCompleteCallback(
        webrtc::DecodedImageCallback *callback) override;

    int32_t Release() override;

    int32_t Decode(const webrtc::EncodedImage &frame, bool missing_frames,
                   int64_t render_time_ms) override;

  private:
    int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx);
    int do_decode(const webrtc::EncodedImage &image,
                      int64_t render_time_ms);

  private:
    // external resources
    webrtc::DecodedImageCallback *callback_ = nullptr;
    // internal resources
    webrtc::H264BitstreamParser h264_bit_stream_parser_;
    const AVCodec *codec_ = nullptr;
    AVCodecContext *avctx_ = nullptr;
    AVCodecParserContext *parser_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVPacket *packet_ = nullptr;
    rtc::scoped_refptr<webrtc::I420Buffer> buffer_ = nullptr;
};
