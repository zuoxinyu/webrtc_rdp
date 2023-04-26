#pragma once
#include "modules/video_coding/codecs/h264/include/h264.h"

static inline constexpr webrtc::ScalabilityMode IkSupportedScalabilityModes[] =
    {webrtc::ScalabilityMode::kL1T1, webrtc::ScalabilityMode::kL1T2,
     webrtc::ScalabilityMode::kL1T3};

static inline std::vector<webrtc::SdpVideoFormat>
supported_h264_codecs(bool mode)
{
    // TODO: vainfo?
    return {
        webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
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
                                 webrtc::H264Level::kLevel3_1, "0", mode),
        webrtc::CreateH264Format(webrtc::H264Profile::kProfileHigh,
                                 webrtc::H264Level::kLevel5_1, "1", mode),
        webrtc::CreateH264Format(webrtc::H264Profile::kProfileHigh,
                                 webrtc::H264Level::kLevel5_1, "0", mode),
    };
}
