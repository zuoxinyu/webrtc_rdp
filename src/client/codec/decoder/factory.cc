#include "factory.hh"
#include "client/codec/h264.hh"
#include "h264_ffmpeg.hh"

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
