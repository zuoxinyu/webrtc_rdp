#include "h264_vaapi.hh"
#include "logger.hh"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"

#ifdef _MSC_VER
#undef av_err2str
#define av_err2str(r) (r)
#endif

using webrtc::VideoCodecMode;
using webrtc::VideoCodecType;
using webrtc::VideoFrameType;

FFMPEGEncoder::FFMPEGEncoder(const webrtc::SdpVideoFormat &format)
{
    logger::debug("create encoder, format: {}", format.ToString());

    {
        std::vector<std::string> devices;
        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        do {
            type = av_hwdevice_iterate_types(type);
            if (type != AV_HWDEVICE_TYPE_NONE)
                devices.emplace_back(av_hwdevice_get_type_name(type));
        } while (type != AV_HWDEVICE_TYPE_NONE);
        logger::debug("supported hardware encoding devices: {}", devices);
    }
}

FFMPEGEncoder::~FFMPEGEncoder() { Release(); };

void FFMPEGEncoder::SetRates(const RateControlParameters &parameters) {}

int32_t FFMPEGEncoder::Release()
{
    // todo
    if (avctx_) {
        avcodec_send_frame(avctx_, nullptr);
        av_frame_free(&swframe_);
        av_frame_free(&hwframe_);
        av_packet_free(&packet_);
        avcodec_free_context(&avctx_);
    }

    return WEBRTC_VIDEO_CODEC_OK;
}

int FFMPEGEncoder::InitEncode(const webrtc::VideoCodec *codec_settings,
                              const webrtc::VideoEncoder::Settings &settings)
{
    logger::debug("codec settings: [\n"
                  "active: {}\n"
                  "qpMax: {}\n"
                  "startBitrate: {}\n"
                  "minBitrate: {}\n"
                  "maxBitrate: {}\n"
                  "maxFramerate: {}\n"
                  "simucastN: {}\n"
                  "gopsize: {}\n"
                  //"dropEnabled: {}\n"
                  //"encodeComplexity: {}\n"
                  "]",
                  codec_settings->active,       //
                  codec_settings->qpMax,        //
                  codec_settings->startBitrate, //
                  codec_settings->minBitrate,   //
                  codec_settings->maxBitrate,   //
                  codec_settings->maxFramerate, //
                  codec_settings->numberOfSimulcastStreams,
                  codec_settings->H264().keyFrameInterval

    );
    if (codec_settings->codecType != VideoCodecType::kVideoCodecH264) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    width_ = codec_settings->width;
    height_ = codec_settings->height;

    auto device = hwac_ ? kDeviceVAAPI : kDeviceX264;

    codec_ = avcodec_find_encoder_by_name(device);
    if (!codec_) {
        logger::error("failed to find  encoder: {}", device);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    avctx_ = avcodec_alloc_context3(codec_);
    if (!avctx_) {
        logger::error("failed to alloc codec context");
        return WEBRTC_VIDEO_CODEC_MEMORY;
    }

    avctx_->width = width_;
    avctx_->height = height_;
    avctx_->pix_fmt = hwac_ ? AV_PIX_FMT_VAAPI : AV_PIX_FMT_YUV420P;
    avctx_->framerate = AVRational{60, 1};
    // AVRational{static_cast<int>(codec_settings->maxFramerate), 1};
    avctx_->time_base = av_inv_q(avctx_->framerate);
    avctx_->gop_size = codec_settings->H264().keyFrameInterval;
    avctx_->max_b_frames = 0;
    avctx_->bit_rate =
        200 * 1000 * 1000; // codec_settings->startBitrate * 1000;
    avctx_->rc_max_rate =
        1000 * 1000 * 1000; // codec_settings->maxBitrate * 1000;
    avctx_->rc_min_rate =
        50 * 1000 * 1000;        // codec_settings->minBitrate * 1000;
    avctx_->global_quality = 10; // 1-100, higher is worse
    avctx_->profile = FF_PROFILE_H264_HIGH;
    avctx_->level = 51; // 5.1

    int ret;
    if (hwac_) {
        // constant quality
        av_opt_set(avctx_->priv_data, "rc_mode", "CQP", 0);
        ret = av_hwdevice_ctx_create(&device_ctx_, AV_HWDEVICE_TYPE_VAAPI,
                                     nullptr, nullptr, 0);
        if (ret < 0) {
            logger::error("failed to create hardware device context: {}",
                          av_err2str(ret));
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
        ret = set_hwframe_ctx(avctx_, device_ctx_);
        if (ret < 0) {
            logger::error("failed to create hardware frame context: {}",
                          av_err2str(ret));
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
    }

    ret = avcodec_open2(avctx_, codec_, nullptr);
    if (ret < 0) {
        logger::error("failed to open codec: {}", av_err2str(ret));
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    swframe_ = av_frame_alloc();
    hwframe_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    hwframe_->width = swframe_->width = width_;
    hwframe_->height = swframe_->height = height_;
    swframe_->format = AV_PIX_FMT_YUV420P;
    ret = av_frame_get_buffer(swframe_, 0);
    if (ret < 0) {
        logger::error("failed to alloc software frame buffer: {}",
                      av_err2str(ret));
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (hwac_) {
        ret = av_hwframe_get_buffer(avctx_->hw_frames_ctx, hwframe_, 0);
        if (ret < 0 || !hwframe_->hw_frames_ctx) {
            logger::error("failed to alloc hardware frame buffer: {}",
                          av_err2str(ret));
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
    }

    if (hwac_) {
        AVPixelFormat *fmts;
        int ret = av_hwframe_transfer_get_formats(
            hwframe_->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_TO, &fmts,
            0);
        if (ret < 0) {
            logger::error("failed to get supported source fmt");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }

        std::vector<std::string> fmt_strs;
        for (AVPixelFormat *f = fmts; *f != AV_PIX_FMT_NONE; f++) {
            fmt_strs.emplace_back(av_get_pix_fmt_name(*f));
        }
        logger::info("supported hardware frame source fmt: {}", fmt_strs);
        av_free(fmts);
    }

    logger::debug("init encoder ok, start encoding");
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
FFMPEGEncoder::Encode(const webrtc::VideoFrame &frame,
                      const std::vector<webrtc::VideoFrameType> *frame_types)
{
    int ret;
    AVFrame *inframe = swframe_;
    intoAVFrame(swframe_, frame);
    if (hwac_) {
        inframe = hwframe_;
        ret = av_hwframe_transfer_data(hwframe_, swframe_, 0);
        if (ret < 0) {
            logger::error("failed to transfer to hardware frame buffer: {}",
                          av_err2str(ret));
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
    }
    ret = avcodec_send_frame(avctx_, inframe);
    if (ret == AVERROR(EAGAIN)) {
        callback_->OnDroppedFrame(
            webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
        return WEBRTC_VIDEO_CODEC_TIMEOUT; // ??
    } else if (ret < 0) {
        logger::warn("failed to send frame: {}", av_err2str(ret));
        return WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
    }

    while (ret == 0) {
        ret = avcodec_receive_packet(avctx_, packet_);
        if (ret < 0) {
            break;
        }
        webrtc::EncodedImage img;
        intoEncodedImage(img, packet_, frame);
        webrtc::CodecSpecificInfo info;
        info.codecType = webrtc::kVideoCodecH264;
        info.codecSpecific.H264.base_layer_sync = false; //?
        info.codecSpecific.H264.packetization_mode =
            webrtc::H264PacketizationMode::NonInterleaved; //?
        info.codecSpecific.H264.idr_frame =
            img._frameType == webrtc::VideoFrameType::kVideoFrameKey;
        auto result = callback_->OnEncodedImage(img, &info);
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FFMPEGEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback *callback)
{
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

webrtc::VideoEncoder::EncoderInfo FFMPEGEncoder::GetEncoderInfo() const
{
    EncoderInfo info;
    info.implementation_name = "h264_hw_encoder";
    info.supports_simulcast = false;
    info.preferred_pixel_formats = {webrtc::VideoFrameBuffer::Type::kI420};
    info.is_hardware_accelerated = hwac_;

    return info;
}

int FFMPEGEncoder::intoAVFrame(AVFrame *swframe,
                               const webrtc::VideoFrame &frame)
{
    auto buffer = frame.video_frame_buffer();
    assert(buffer->type() == webrtc::VideoFrameBuffer::Type::kI420);
    auto yuv = buffer->GetI420();
    swframe->linesize[0] = yuv->StrideY();
    swframe->linesize[1] = yuv->StrideU();
    swframe->linesize[2] = yuv->StrideV();
    swframe->data[0] = const_cast<uint8_t *>(yuv->DataY());
    swframe->data[1] = const_cast<uint8_t *>(yuv->DataU());
    swframe->data[2] = const_cast<uint8_t *>(yuv->DataV());
    // no need to scale
    return 0;
}

int FFMPEGEncoder::intoEncodedImage(webrtc::EncodedImage &image,
                                    const AVPacket *pkt,
                                    const webrtc::VideoFrame &frame)
{
    auto buf = webrtc::EncodedImageBuffer::Create(pkt->data, pkt->size);
    image.SetEncodedData(buf);
    // rtpfragment? -- no need for libavcodec, see:
    // https://stackoverflow.com/questions/45632432/webrtc-what-is-rtpfragmentationheader-in-encoder-implementation

    h264_bit_stream_parser_.ParseBitstream(image);
    image.qp_ = h264_bit_stream_parser_.GetLastSliceQp().value_or(-1);
    image._encodedWidth = width_;
    image._encodedHeight = height_;
    image._frameType = pkt->flags & AV_PKT_FLAG_KEY
                           ? webrtc::VideoFrameType::kVideoFrameKey
                           : webrtc::VideoFrameType::kVideoFrameDelta;
    image.ntp_time_ms_ = frame.ntp_time_ms();
    image.capture_time_ms_ = frame.render_time_ms();
    image.rotation_ = frame.rotation();
    image.content_type_ = webrtc::VideoContentType::SCREENSHARE;
    image.SetTimestamp(frame.timestamp());
    image.SetColorSpace(frame.color_space());

    return 0;
}

int FFMPEGEncoder::set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *device_ctx)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = nullptr;
    int ret = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(device_ctx))) {
        logger::error("Failed to create VAAPI frame context.");
        return -1;
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width = width_;
    frames_ctx->height = height_;
    frames_ctx->initial_pool_size = 20;
    ret = av_hwframe_ctx_init(hw_frames_ref);
    if (ret < 0) {
        logger::error("Failed to initialize VAAPI frame context."
                      "Error code: {}",
                      av_err2str(ret));
        av_buffer_unref(&hw_frames_ref);
        return ret;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx)
        ret = AVERROR(ENOMEM);

    av_buffer_unref(&hw_frames_ref);
    return ret;
}
