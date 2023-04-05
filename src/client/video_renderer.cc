#include "video_renderer.hh"

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <stdexcept>

static const std::string vs_src = R"(
    #version 330 core

    attribute vec2 aPos;
    attribute vec2 aTexCoord;

    varying vec2 vTexCoord;

    void main() {
      gl_Position = vec4(aPos, 0.0, 1.0);
      vTexCoord = aTexCoord;
    }
)";
static const std::string fs_src = R"(
    #version 330 core

    varying vec2 vTexCoord;

    uniform sampler2D uTexY;
    uniform sampler2D uTexU;
    uniform sampler2D uTexV;

    void main() {
      vec3 yuv;
      vec3 rgb;
      yuv.x = texture2D(uTexY, vTexCoord).r;
      yuv.y = texture2D(uTexU, vTexCoord).r - 0.5;
      yuv.z = texture2D(uTexV, vTexCoord).r - 0.5;
      rgb = mat3( 1,       1,         1,
                  0,       -0.39465,  2.03211,
                  1.13983, -0.58060,  0) * yuv;
      gl_FragColor = vec4(rgb, 1);
    }
)";
static const float pos_buffer[] = {
    -1.0, +1.0, // lt
    +1.0, +1.0, // rt
    -1.0, -1.0, // lb
    +1.0, -1.0, // rb
};
static const float tex_buffer[] = {
    0.0, 0.0, // lt
    1.0, 0.0, // rt
    0.0, 1.0, // lb
    1.0, 1.0, // rb
};

VideoRenderer::VideoRenderer(SDL_Window *win) : window_(win)
{
    window_ = SDL_CreateWindow("video window", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, 2560, 1440,
                               SDL_WINDOW_OPENGL);
    glctx_ = SDL_GL_CreateContext(window_);

    if (glewInit() != GLEW_OK) {
        throw std::runtime_error("glew init failed");
    }

    SDL_GetWindowSize(window_, &width_, &height_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    program_ = create_program(vs_src, fs_src);
    glUseProgram(program_);

    int tex = glGetAttribLocation(program_, "aTexCoord");
    int pos = glGetAttribLocation(program_, "aPos");

    pos_buffer_ =
        create_buffer(pos, &pos_buffer[0], sizeof(pos_buffer) / sizeof(float));
    tex_buffer_ =
        create_buffer(tex, &tex_buffer[0], sizeof(tex_buffer) / sizeof(float));

    texture_[Y] = create_texture();
    texture_[U] = create_texture();
    texture_[V] = create_texture();
}

VideoRenderer::~VideoRenderer()
{
    glDeleteProgram(program_);

    glDeleteBuffers(1, &pos_buffer_);
    glDeleteBuffers(1, &tex_buffer_);

    glDeleteTextures(1, &texture_[V]);
    glDeleteTextures(1, &texture_[U]);
    glDeleteTextures(1, &texture_[Y]);
}

GLuint VideoRenderer::create_texture()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

GLuint VideoRenderer::create_shader(uint typ, const std::string &code)
{
    const char *src = code.c_str();
    GLuint shader = glCreateShader(typ);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (!status) {
        char logbuf[512] = {0};
        glGetShaderInfoLog(shader, 512, nullptr, logbuf);
        std::cerr << "shader compile failed: " << logbuf << std::endl;
        exit(EXIT_FAILURE);
    }

    return shader;
}

GLuint VideoRenderer::create_program(const std::string &vs,
                                     const std::string &fs)
{
    auto vsShader = create_shader(GL_VERTEX_SHADER, vs);
    auto fsShader = create_shader(GL_FRAGMENT_SHADER, fs);
    GLuint program = glCreateProgram();

    glAttachShader(program, vsShader);
    glAttachShader(program, fsShader);
    glLinkProgram(program);
    glDetachShader(program, vsShader);
    glDetachShader(program, fsShader);
    glDeleteShader(vsShader);
    glDeleteShader(fsShader);

    int status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char logbuf[512] = {0};
        glGetProgramInfoLog(program, 512, nullptr, logbuf);
        std::cerr << "program link failed: " << logbuf << std::endl;
        exit(EXIT_FAILURE);
    }

    return program;
}

GLuint VideoRenderer::create_buffer(int location, const float data[], size_t sz)
{
    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, sz, data, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(location, 2, GL_FLOAT, false, 8, 0);
    glEnableVertexAttribArray(location);

    return buf;
}

void VideoRenderer::update_textures(const void *ydata, const void *udata,
                                    const void *vdata)
{
    SDL_GL_MakeCurrent(window_, glctx_);

    glBindTexture(GL_TEXTURE_2D, texture_[Y]);
    glActiveTexture(GL_TEXTURE0 + Y);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, ydata);
    glUniform1i(glGetUniformLocation(program_, "uTexY"), Y);

    glBindTexture(GL_TEXTURE_2D, texture_[U]);
    glActiveTexture(GL_TEXTURE0 + U);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ / 2, height_ / 2, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, udata);
    glUniform1i(glGetUniformLocation(program_, "uTexU"), U);

    glBindTexture(GL_TEXTURE_2D, texture_[V]);
    glActiveTexture(GL_TEXTURE0 + V);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ / 2, height_ / 2, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, vdata);
    glUniform1i(glGetUniformLocation(program_, "uTexV"), V);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SDL_GL_SwapWindow(window_);
}

// running on capture thread?
void VideoRenderer::OnFrame(const webrtc::VideoFrame &frame)
{
    auto yuv = frame.video_frame_buffer()->GetI420();
    // clang-format off
    std::cout << "get frame: ["
              << " id="          << frame.id()
              << " size="        << frame.size()
              << " width="       << frame.width()
              << " height="      << frame.height()
              << " timestamp="   << frame.timestamp()
              << " ntp time="    << frame.ntp_time_ms()
              << " render time=" << frame.render_time_ms()
              << "]" << std::endl;
    // clang-format on

    update_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
}
