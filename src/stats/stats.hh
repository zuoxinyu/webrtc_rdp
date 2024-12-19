#pragma once

#include "api/make_ref_counted.h"
#include "api/stats/rtc_stats_collector_callback.h"

#include <nlohmann/json.hpp>

class StatsObserver : public webrtc::RTCStatsCollectorCallback
{
    using json = nlohmann::ordered_json;

  public:
    static webrtc::scoped_refptr<StatsObserver> Create(std::string &json)
    {
        return  webrtc::make_ref_counted<StatsObserver>(json);
    }

    StatsObserver(std::string &json) : json_(json) {}

    void OnStatsDelivered(
        const webrtc::scoped_refptr<const webrtc::RTCStatsReport> &report) override
    {
        auto json = report->ToJson();

        json_ = dump_section(json, "inbound-rtp");
        json_ += dump_section(json, "outbound-rtp");
    }

    static std::string dump_section(const std::string &json,
                                    const std::string &section)
    {
        auto arr = json::parse(json);
        auto findfn = [&section](const decltype(arr) &o) -> bool {
            return o["type"] == section && o["kind"] == "video";
        };
        auto it = std::find_if(std::begin(arr), std::end(arr), findfn);
        if (it == arr.end()) {
            return {};
        }

        /* logger::debug("stats\n{}", it->dump(4)); */
        return it->dump(4);
    }

  private:
    std::string &json_;
};
