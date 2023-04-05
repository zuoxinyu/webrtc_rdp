#pragma once

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

struct VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
  public:
    explicit VideoRenderer(SDL_Window *);
    ~VideoRenderer();

  private:
    enum { Y = 0, U = 1, V = 2 };
    GLuint create_texture();
    GLuint create_buffer(int location, const float data[], size_t sz);
    GLuint create_shader(uint typ, const std::string &code);
    GLuint create_program(const std::string &vs, const std::string &fs);
    void update_textures(const void *ydata, const void *udata,
                         const void *vdata);

  public:
    void OnFrame(const webrtc::VideoFrame &frame) override;

  private:
    SDL_Window *window_;
    SDL_GLContext glctx_;
    GLuint texture_[3];
    GLuint program_;
    GLuint tex_buffer_;
    GLuint pos_buffer_;
    int width_ = 0;
    int height_ = 0;
};
