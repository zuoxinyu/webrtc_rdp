#include "factory.hh"
#include "h264_ffmpeg.hh"

#include "api/video_codecs/video_decoder_factory_template.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

using Factory = webrtc::VideoDecoderFactoryTemplate<FFMPEGDecoder>;

static constexpr webrtc::ScalabilityMode IkSupportedScalabilityModes[] = {
    webrtc::ScalabilityMode::kL1T1, webrtc::ScalabilityMode::kL1T2,
    webrtc::ScalabilityMode::kL1T3};

static std::vector<webrtc::SdpVideoFormat> supported_h264_codecs(bool mode)
{
    return {webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                                     webrtc::H264Level::kLevel3_1, "1", mode),
            webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                                     webrtc::H264Level::kLevel3_1, "0", mode),
            webrtc::CreateH264Format(
                webrtc::H264Profile::kProfileConstrainedBaseline,
                webrtc::H264Level::kLevel3_1, "1", mode),
            webrtc::CreateH264Format(
                webrtc::H264Profile::kProfileConstrainedBaseline,
                webrtc::H264Level::kLevel3_1, "0", mode),
            webrtc::CreateH264Format(webrtc::H264Profile::kProfileMain,
                                     webrtc::H264Level::kLevel3_1, "1", mode),
            webrtc::CreateH264Format(webrtc::H264Profile::kProfileMain,
                                     webrtc::H264Level::kLevel3_1, "0", mode)};
}

std::vector<webrtc::SdpVideoFormat>
CustomVideoDecoderFactory::GetSupportedFormats() const
{
    return supported_h264_codecs(true);
}

std::unique_ptr<webrtc::VideoDecoder>
CustomVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat &format)
{
    return std::make_unique<FFMPEGDecoder>(format);
}
