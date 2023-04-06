#pragma once

#include <boost/lockfree/policies.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/sync_queue.hpp>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

struct VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
  public:
    explicit VideoRenderer(SDL_Window *);
    explicit VideoRenderer();
    ~VideoRenderer();
    void update_frame();

  private:
    enum { Y = 0, U = 1, V = 2 };
    GLuint create_texture();
    GLuint create_buffer(int location, const float data[], size_t sz);
    GLuint create_shader(uint typ, const std::string &code);
    GLuint create_program(const std::string &vs, const std::string &fs);
    void update_gl_textures(const void *ydata, const void *udata,
                            const void *vdata);
    void update_sdl_textures(const void *ydata, const void *udata,
                             const void *vdata);

  public:
    void OnFrame(const webrtc::VideoFrame &frame) override;

  private:
    SDL_Window *window_ = nullptr;
    SDL_GLContext glctx_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    GLuint textures_[3] = {0, 0, 0};
    GLuint program_ = 0;
    GLuint tex_buffer_ = 0;
    GLuint pos_buffer_ = 0;
    int width_ = 0;
    int height_ = 0;

    boost::sync_queue<rtc::scoped_refptr<webrtc::VideoFrameBuffer>>
        frame_queue_;
    /* boost::lockfree::queue<webrtc::VideoFrameBuffer *, */
    /*                        boost::lockfree::capacity<16>> */
    /*     frame_queue_; */
};
