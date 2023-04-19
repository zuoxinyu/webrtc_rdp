#pragma once

#include "logger.hh"

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include "api/peer_connection_interface.h"

namespace json = boost::json;

class StatsObserver : public webrtc::RTCStatsCollectorCallback
{
  public:
    static rtc::scoped_refptr<StatsObserver> Create(std::string &json)
    {
        return rtc::make_ref_counted<StatsObserver>(json);
    }

    StatsObserver(std::string &json) : json_(json) {}

    void OnStatsDelivered(
        const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report) override
    {
        auto json = report->ToJson();

        dump_section(json, "outbound-rtp");
        json_ = dump_section(json, "inbound-rtp");
    }

    static std::string dump_section(const std::string &json,
                                    const std::string &section)
    {
        std::string content;
        auto arr = json::parse(json).as_array();
        auto it =
            std::find_if(arr.cbegin(), arr.cend(),
                         [&section](const boost::json::value &v) -> bool {
                             auto o = v.as_object();
                             return o.at("type").get_string() == section &&
                                    o.contains("kind") &&
                                    o.at("kind").get_string() == "video";
                         });
        if (!it->is_object()) {
            return content;
        }
        auto obj = it->as_object();
        std::for_each(obj.cbegin(), obj.cend(),
                      [&content](boost::json::key_value_pair v) {
                          content += std::string(v.key()) + ": " +
                                     json::serialize(v.value()) + "\n";
                      });

        // logger::debug("stats section [type={}]:\n{}", section, content);
        return content;
    }

  private:
    std::string &json_;
};
