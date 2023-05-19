#include "factory.hh"
#include "codec/h264.hh"
#include "h264_vaapi.hh"

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
