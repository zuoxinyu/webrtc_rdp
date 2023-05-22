#pragma once

#include "video_sink.hh"

// #include <queue>
#include <boost/thread/sync_queue.hpp>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "api/video/video_frame.h"
#include "modules/desktop_capture/desktop_capture_types.h"

struct VideoRenderer : public VideoSink {
  public:
    struct Config {
        std::string name = "video sink window";
        int width = 800;
        int height = 600;
        bool use_opengl = false;
        bool dump = false;
        bool hide = true;
    };

    using FramePtr = rtc::scoped_refptr<webrtc::VideoFrameBuffer>;
    using FrameQueue = boost::sync_queue<FramePtr>;

  public:
    static rtc::scoped_refptr<VideoRenderer> Create(Config conf);
    ~VideoRenderer() override;
    SDL_Window *get_window() const { return window_; }
    /* webrtc::WindowId get_native_window_handle() const; */
    void update_frame();

    // impl VideoSinkInterface
    void OnFrame(const webrtc::VideoFrame &frame) override;
    // impl VideoSink
    void Start() override;
    void Stop() override;

    // TODO: CRTP?
    virtual void update_textures(const void *ydata, const void *udata,
                                 const void *vdata) = 0;

  protected:
    explicit VideoRenderer(Config conf);

  private:
    void dump_frame(FramePtr frame, int id = 0);

  protected:
    // resources
    SDL_Window *window_ = nullptr;
    // properties
    Config conf_;
    // states
    bool running_ = false;

    FramePtr last_frame_;
    FrameQueue frame_queue_;
};
