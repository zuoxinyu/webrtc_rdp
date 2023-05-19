#pragma once
#include "video_renderer.hh"

struct OpenGLRenderer : public VideoRenderer {
  public:
    OpenGLRenderer(Config conf);
    ~OpenGLRenderer() override;

  private:
    void update_textures(const void *ydata, const void *udata,
                         const void *vdata) override;
    enum { Y = 0, U = 1, V = 2 };
    GLuint create_texture();
    GLuint create_buffer(int location, const float data[], size_t sz);
    GLuint create_shader(unsigned typ, const std::string &code);
    GLuint create_program(const std::string &vs, const std::string &fs);

  private:
    // resources
    SDL_GLContext glctx_ = nullptr;
    GLuint textures_[3] = {0, 0, 0};
    GLuint vao, vbo, ebo;
    GLuint program_ = 0;
};
