#include "video_renderer.hh"
#include "logger.hh"

#include <cstdio>
#include <stdexcept>

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_version.h>
#include <SDL2/SDL_video.h>

static const std::string vs_src = R"(
    #version 330 core

    layout (location = 0) in vec2 aPosition;
    layout (location = 1) in vec2 aTexCoord;

    varying vec2 vTexCoord;

    void main() {
      gl_Position = vec4(aPosition, 0.0, 1.0);
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
static const float vertices[] = {
    // position|texcoord
    -1.0, -1.0, 0.0, 1.0, // lt
    +1.0, -1.0, 1.0, 1.0, // rt
    -1.0, +1.0, 0.0, 0.0, // lb
    +1.0, +1.0, 1.0, 0.0, // rb
};

static const int indices[] = {
    0, 1, 2, //
    1, 2, 3, //
};

rtc::scoped_refptr<VideoRenderer> VideoRenderer::Create(Config conf)
{
    if (conf.use_opengl) {
        return rtc::make_ref_counted<VideoRenderer>(std::move(conf), nullptr);
    } else {
        return rtc::make_ref_counted<VideoRenderer>(std::move(conf));
    }
}

VideoRenderer::VideoRenderer(Config conf) : conf_(std::move(conf))
{
    running_ = !conf.hide;
    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (conf_.hide)
        flags |= SDL_WINDOW_HIDDEN;

    window_ = SDL_CreateWindow(conf_.name.c_str(), 0, 100, conf_.width,
                               conf_.height, flags);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV,
                                 SDL_TEXTUREACCESS_STREAMING, conf_.width,
                                 conf_.height);
    SDL_SetRenderDrawColor(renderer_, 0, 128, 128, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);
}

VideoRenderer::VideoRenderer(Config conf, SDL_Window *win)
    : window_(win), conf_(std::move(conf))
{
    running_ = !conf.hide;
    uint32_t flags = SDL_WINDOW_OPENGL;
    if (conf_.hide)
        flags |= SDL_WINDOW_HIDDEN;

    window_ = SDL_CreateWindow(conf_.name.c_str(), 0, 100, conf_.width,
                               conf_.height, flags);
    glctx_ = SDL_GL_CreateContext(window_);
    if (glewInit() != GLEW_OK) {
        throw std::runtime_error("glew init failed");
    }
    assert(glctx_);

    glViewport(0, 0, conf_.width, conf_.height);

    program_ = create_program(vs_src, fs_src);
    glUseProgram(program_);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, false, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, false, 4 * sizeof(float), (void *)8);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    textures_[Y] = create_texture();
    textures_[U] = create_texture();
    textures_[V] = create_texture();

    glBindVertexArray(vao);
    glUseProgram(program_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    SDL_GL_SwapWindow(window_);
}

VideoRenderer::~VideoRenderer()
{
    if (conf_.use_opengl) {
        glDeleteProgram(program_);

        glDeleteBuffers(1, &pos_buffer_);
        glDeleteBuffers(1, &tex_buffer_);

        glDeleteTextures(1, &textures_[V]);
        glDeleteTextures(1, &textures_[U]);
        glDeleteTextures(1, &textures_[Y]);

        SDL_GL_DeleteContext(glctx_);
    } else {
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(renderer_);
    }
    SDL_DestroyWindow(window_);
}

void VideoRenderer::Start()
{
    running_ = true;
    SDL_ShowWindow(window_);
}

void VideoRenderer::Stop()
{
    running_ = false;
    SDL_HideWindow(window_);
}

webrtc::WindowId VideoRenderer::get_native_window_handle() const
{
    auto sdlid = SDL_GetWindowID(window_);
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version)
    SDL_GetWindowWMInfo(window_, &info);
    return info.info.x11.window;
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

void VideoRenderer::update_gl_textures(const void *ydata, const void *udata,
                                       const void *vdata)
{
    SDL_GL_MakeCurrent(window_, glctx_);

    glActiveTexture(GL_TEXTURE0 + Y);
    glBindTexture(GL_TEXTURE_2D, textures_[Y]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, conf_.width, conf_.height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, ydata);
    glUniform1i(glGetUniformLocation(program_, "uTexY"), Y);

    glActiveTexture(GL_TEXTURE0 + U);
    glBindTexture(GL_TEXTURE_2D, textures_[U]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, conf_.width / 2,
                 conf_.height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, udata);
    glUniform1i(glGetUniformLocation(program_, "uTexU"), U);

    glActiveTexture(GL_TEXTURE0 + V);
    glBindTexture(GL_TEXTURE_2D, textures_[V]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, conf_.width / 2,
                 conf_.height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, vdata);
    glUniform1i(glGetUniformLocation(program_, "uTexV"), V);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    SDL_GL_SwapWindow(window_);
}

void VideoRenderer::update_sdl_textures(const void *ydata, const void *udata,
                                        const void *vdata)
{
    // TODO: use SDL_LockTexture instead?
    SDL_UpdateYUVTexture(texture_, nullptr, //
                         static_cast<const uint8_t *>(ydata), conf_.width,
                         static_cast<const uint8_t *>(udata), conf_.width / 2,
                         static_cast<const uint8_t *>(vdata), conf_.width / 2);
    SDL_Rect rect{0, 0, conf_.width, conf_.height};
    SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
    SDL_RenderPresent(renderer_);
    SDL_RenderFlush(renderer_);
}

// running on capture thread (local) or worker thread (remote)?
void VideoRenderer::OnFrame(const webrtc::VideoFrame &frame)
{
    static int once = 0;
    if (conf_.dump && once < 1200 && once % 60 == 0) {
        dump_frame(frame, once);
    }
    once++;

    frame_queue_.try_push(frame.video_frame_buffer());
}

void VideoRenderer::update_frame()
{
    if (!running_)
        return;

    if (conf_.hide) {
        SDL_ShowWindow(window_);
        conf_.hide = false;
    }

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame = nullptr;
    frame_queue_.try_pull(frame);
    if (!frame) {
        return;
    }

    auto scaled = frame->Scale(conf_.width, conf_.height);
    auto yuv = scaled->GetI420();
    if (yuv) {
        if (conf_.use_opengl)
            update_gl_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
        else
            update_sdl_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
    }
}

void VideoRenderer::dump_frame(const webrtc::VideoFrame &frame, int id)
{
    auto buf = frame.video_frame_buffer();
    auto yuv = buf->GetI420();
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
