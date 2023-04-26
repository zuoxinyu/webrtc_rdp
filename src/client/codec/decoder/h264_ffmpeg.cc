#include "h264_ffmpeg.hh"
#include "logger.hh"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
}

#include "api/video_codecs/video_codec.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/time_utils.h"

FFMPEGDecoder::FFMPEGDecoder(const webrtc::SdpVideoFormat &format)
{
    logger::debug("create decoder, format: {}", format.ToString());
    // TODO: parse profile/level
}

FFMPEGDecoder::~FFMPEGDecoder() { Release(); }

bool FFMPEGDecoder::Configure(const webrtc::VideoDecoder::Settings &settings)
{
    codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    parser_ = av_parser_init(codec_->id);
    avctx_ = avcodec_alloc_context3(codec_);
    int ret = avcodec_open2(avctx_, codec_, nullptr);
    if (ret < 0) {
        logger::error("failed to open codec: {}", "h264");
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();

    logger::debug("init decoder ok, start decoding");
    return true;
}

int32_t FFMPEGDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback *callback)
{
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FFMPEGDecoder::Release()
{
    if (avctx_) {
        avcodec_send_packet(avctx_, nullptr);
        av_frame_free(&frame_);
        av_packet_free(&packet_);
        av_parser_close(parser_);
        avcodec_free_context(&avctx_);
    }

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FFMPEGDecoder::Decode(const webrtc::EncodedImage &image,
                              bool missing_frames, int64_t render_time_ms)
{
    int ret;
    auto *data = const_cast<uint8_t *>(image.data());
    auto size = image.size();

    while (size) {
        ret = av_parser_parse2(parser_, avctx_, &packet_->data, &packet_->size,
                               data, size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        data += ret;
        size -= ret;
        if (packet_->size == 0) {
            continue;
        }

        ret = do_decode(image, render_time_ms);
        if (ret < 0) {
            return ret;
        }
    }

    return WEBRTC_VIDEO_CODEC_OK;
}

int FFMPEGDecoder::do_decode(const webrtc::EncodedImage &image,
                             int64_t render_time_ms)
{
    int ret = avcodec_send_packet(avctx_, packet_);
    if (ret < 0) {
        logger::warn("failed to send packet: {}", av_err2str(ret));
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    while (true) {
        ret = avcodec_receive_frame(avctx_, frame_);
        if (ret < 0) {
            break;
        }

        assert(frame_->format == AV_PIX_FMT_YUV420P);
        buffer_ =
            webrtc::I420Buffer::Copy(frame_->width, frame_->height,        //
                                     frame_->data[0], frame_->linesize[0], //
                                     frame_->data[1], frame_->linesize[1], //
                                     frame_->data[2], frame_->linesize[2]);

        webrtc::VideoFrame frame(buffer_, webrtc::kVideoRotation_0,
                                 render_time_ms *
                                     rtc::kNumMicrosecsPerMillisec);
        frame.set_timestamp(image.Timestamp());
        frame.set_ntp_time_ms(image.NtpTimeMs());

        callback_->Decoded(frame);
    }

    return WEBRTC_VIDEO_CODEC_OK;
}

webrtc::VideoDecoder::DecoderInfo FFMPEGDecoder::GetDecoderInfo() const
{
    DecoderInfo info;
    info.implementation_name = "h264_ffmpeg_decoder";
    info.is_hardware_accelerated = false;
    return info;
}
