#include "video_renderer.hh"
#include "logger.hh"

#include <cstdio>
#include <stdexcept>

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_video.h>

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

constexpr int kWidth = 1920;
constexpr int kHeight = 1080;

VideoRenderer::VideoRenderer()
{
    window_ = SDL_CreateWindow("video window", 0, 100, kWidth, kHeight,
                               SDL_WINDOW_OPENGL);
    SDL_GetWindowSize(window_, &width_, &height_);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV,
                                 SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
    SDL_SetRenderDrawColor(renderer_, 0, 128, 128, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);
}

VideoRenderer::VideoRenderer(SDL_Window *win) : window_(win)
{
    window_ = SDL_CreateWindow("video window", kWidth, kHeight, kWidth, kHeight,
                               SDL_WINDOW_OPENGL);

    glctx_ = SDL_GL_CreateContext(window_);

    if (glewInit() != GLEW_OK) {
        throw std::runtime_error("glew init failed");
    }

    SDL_GetWindowSize(window_, &width_, &height_);

    glViewport(0, 0, width_, height_);
    /* glClearColor(0, 0.2, 0.2, 1); */
    /* glClearDepth(1.0); */
    /* glClear(GL_COLOR_BUFFER_BIT); */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    program_ = create_program(vs_src, fs_src);
    glUseProgram(program_);

    int tex = glGetAttribLocation(program_, "aTexCoord");
    int pos = glGetAttribLocation(program_, "aPos");

    pos_buffer_ =
        create_buffer(pos, &pos_buffer[0], sizeof(pos_buffer) / sizeof(float));
    tex_buffer_ =
        create_buffer(tex, &tex_buffer[0], sizeof(tex_buffer) / sizeof(float));

    textures_[Y] = create_texture();
    textures_[U] = create_texture();
    textures_[V] = create_texture();

    /* glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); */
    /* SDL_GL_SwapWindow(window_); */
}

VideoRenderer::~VideoRenderer()
{
    glDeleteProgram(program_);

    glDeleteBuffers(1, &pos_buffer_);
    glDeleteBuffers(1, &tex_buffer_);

    glDeleteTextures(1, &textures_[V]);
    glDeleteTextures(1, &textures_[U]);
    glDeleteTextures(1, &textures_[Y]);
}

void VideoRenderer::update_frame()
{
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame = nullptr;
    frame_queue_.nonblocking_pull(frame);
    if (frame) {
        auto yuv = frame->GetI420();
        if (yuv) {

            if (glctx_)
                update_gl_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
            else
                update_sdl_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
        }
    }
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

GLuint VideoRenderer::create_shader(unsigned typ, const std::string &code)
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
        logger::error("shader compile failed: {}", logbuf);
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
        logger::error("program link failed: {}", logbuf);
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
    glVertexAttribPointer(location, 2, GL_FLOAT, false, 8, nullptr);
    glEnableVertexAttribArray(location);

    return buf;
}

void VideoRenderer::update_sdl_textures(const void *ydata, const void *udata,
                                        const void *vdata)
{
    // TODO: use SDL_LockTexture instead?
    SDL_UpdateYUVTexture(texture_, nullptr, //
                         static_cast<const uint8_t *>(ydata), kWidth,
                         static_cast<const uint8_t *>(udata), kWidth / 2,
                         static_cast<const uint8_t *>(vdata), kWidth / 2);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    SDL_RenderFlush(renderer_);
}

void VideoRenderer::dump_frame(const webrtc::VideoFrame &frame, int id)
{
    auto buf = frame.video_frame_buffer();
    auto scaled = buf->Scale(kWidth, kHeight);
    auto yuv = scaled->GetI420();
    char name[20] = {0};
    sprintf(name, "frame-%02d.yuv", id);
    ::FILE *f = ::fopen(name, "wb+");
    fwrite(yuv->DataY(), 1, yuv->StrideY() * yuv->height(), f);
    fwrite(yuv->DataU(), 1, yuv->StrideU() * yuv->height() / 2, f);
    fwrite(yuv->DataV(), 1, yuv->StrideV() * yuv->height() / 2, f);
    fflush(f);
    fclose(f);

    logger::debug("get frame: ["
                  " id={}"
                  " size={}"
                  " width={}"
                  " height={}"
                  " timestamp={}"
                  " ntp time={}"
                  " render time={}"
                  " ]",
                  frame.id(), frame.size(), frame.width(), frame.height(),
                  frame.timestamp(), frame.ntp_time_ms(),
                  frame.render_time_ms());
}

void VideoRenderer::update_gl_textures(const void *ydata, const void *udata,
                                       const void *vdata)
{
    SDL_GL_MakeCurrent(window_, glctx_);

    glBindTexture(GL_TEXTURE_2D, textures_[Y]);
    glActiveTexture(GL_TEXTURE0 + Y);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, ydata);
    glUniform1i(glGetUniformLocation(program_, "uTexY"), Y);

    glBindTexture(GL_TEXTURE_2D, textures_[U]);
    glActiveTexture(GL_TEXTURE0 + U);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ / 2, height_ / 2, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, udata);
    glUniform1i(glGetUniformLocation(program_, "uTexU"), U);

    glBindTexture(GL_TEXTURE_2D, textures_[V]);
    glActiveTexture(GL_TEXTURE0 + V);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ / 2, height_ / 2, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, vdata);
    glUniform1i(glGetUniformLocation(program_, "uTexV"), V);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SDL_GL_SwapWindow(window_);
}

// running on capture thread
void VideoRenderer::OnFrame(const webrtc::VideoFrame &frame)
{
    auto buf = frame.video_frame_buffer();
    auto scaled = buf->Scale(kWidth, kHeight);
    auto yuv = scaled->GetI420();
    static int once = 0;
    if (once < 1200 && once % 60 == 0) {
        dump_frame(frame, once);
    }
    once++;

    update_sdl_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
}
