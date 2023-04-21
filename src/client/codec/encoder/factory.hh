#pragma once
#include <memory>
#include <vector>

#include "api/video_codecs/video_encoder_factory.h"

class CustomVideoEncoderFactory : public webrtc::VideoEncoderFactory
{
  public:
    ~CustomVideoEncoderFactory() override = default;
    std::unique_ptr<webrtc::VideoEncoder>
    CreateVideoEncoder(const webrtc::SdpVideoFormat &format) override;

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

  private:
};
