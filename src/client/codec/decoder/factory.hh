#pragma once
#include <memory>
#include <vector>

#include "api/video_codecs/video_decoder_factory.h"

class CustomVideoDecoderFactory : public webrtc::VideoDecoderFactory
{
  public:
    ~CustomVideoDecoderFactory() override = default;
    std::unique_ptr<webrtc::VideoDecoder>
    CreateVideoDecoder(const webrtc::SdpVideoFormat &format) override;

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

  private:
};
