#include "factory.hh"
#include "codec/h264.hh"
#include "h264_vaapi.hh"

std::vector<webrtc::SdpVideoFormat>
CustomVideoEncoderFactory::GetSupportedFormats() const
{
    return supported_h264_codecs(true);
}

std::unique_ptr<webrtc::VideoEncoder>
CustomVideoEncoderFactory::Create(const webrtc::Environment &env,
                                  const webrtc::SdpVideoFormat &format)
{
    return std::make_unique<FFMPEGEncoder>(format);
}
