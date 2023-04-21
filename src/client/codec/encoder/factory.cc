#include "factory.hh"
#include "h264_vaapi.hh"

#include "api/video_codecs/video_encoder_factory_template.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

using Factory = webrtc::VideoEncoderFactoryTemplate<FFMPEGEncoder>;

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
CustomVideoEncoderFactory::GetSupportedFormats() const
{
    return supported_h264_codecs(true);
}

std::unique_ptr<webrtc::VideoEncoder>
CustomVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat &format)
{
    return std::make_unique<FFMPEGEncoder>(format);
}
