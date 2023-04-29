#pragma once

#include <list>

#include "api/media_stream_interface.h"

class VideoTrackSource : public webrtc::VideoTrackSourceInterface
{
  public:
    explicit VideoTrackSource(bool remote) : remote_(remote) {}
    ~VideoTrackSource() override = default;

    void SetState(SourceState new_state) { state_ = new_state; }

    SourceState state() const override { return state_; }
    bool remote() const override { return remote_; }

    bool is_screencast() const override { return true; }
    absl::optional<bool> needs_denoising() const override
    {
        return absl::nullopt;
    }

    bool GetStats(Stats *stats) override { return false; }

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                         const rtc::VideoSinkWants &wants) override
    {
        source()->AddOrUpdateSink(sink, wants);
    }
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        source()->RemoveSink(sink);
    }

    bool SupportsEncodedOutput() const override { return false; }
    void GenerateKeyFrame() override {}
    void AddEncodedSink(
        rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame> *sink) override
    {
    }
    void RemoveEncodedSink(
        rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame> *sink) override
    {
    }

    void RegisterObserver(webrtc::ObserverInterface *observer) override
    {
        observers_.push_back(observer);
    }
    void UnregisterObserver(webrtc::ObserverInterface *observer) override
    {
        std::erase(observers_, observer);
    }

  public:
    virtual void Start() = 0;
    virtual void Stop() = 0;

  protected:
    virtual rtc::VideoSourceInterface<webrtc::VideoFrame> *source() = 0;

  private:
    std::list<webrtc::ObserverInterface *> observers_;
    SourceState state_{kInitializing};
    const bool remote_;
};

class VideoSource : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{

  public: // impl VideoSourceInterface
    ~VideoSource() override = default;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                         const rtc::VideoSinkWants &wants) override
    {
        auto pair = std::find_if(
            sinks_.begin(), sinks_.end(),
            [sink](const SinkPair &pair) { return pair.sink == sink; });
        if (pair == sinks_.end()) {
            sinks_.emplace_back(sink, wants);
        } else {
            pair->wants = wants;
        }
    };

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
    {
        std::erase_if(
            sinks_, [sink](const SinkPair &pair) { return pair.sink == sink; });
    };

  protected:
    struct SinkPair {
        SinkPair(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink,
                 const rtc::VideoSinkWants &wants)
            : sink(sink), wants(wants)
        {
        }
        rtc::VideoSinkInterface<webrtc::VideoFrame> *sink;
        rtc::VideoSinkWants wants;
    };
    std::vector<SinkPair> sinks_;
};
