#include "video_renderer.hh"
#include "logger.hh"
#include "opengl_renderer.hh"
#include "sdl_renderer.hh"

#include <cstdio>
#include <stdexcept>

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_version.h>
#include <SDL2/SDL_video.h>

rtc::scoped_refptr<VideoRenderer> VideoRenderer::Create(Config conf)
{
    if (conf.use_opengl) {
        return rtc::make_ref_counted<OpenGLRenderer>(std::move(conf));
    } else {
        return rtc::make_ref_counted<SDLRenderer>(std::move(conf));
    }
}

VideoRenderer::VideoRenderer(Config conf) : conf_(std::move(conf))
{
    running_ = !conf.hide;
    uint32_t flags = SDL_WINDOW_OPENGL;// | SDL_WINDOW_ALLOW_HIGHDPI;
    if (conf_.hide)
        flags |= SDL_WINDOW_HIDDEN;

    window_ = SDL_CreateWindow(conf_.name.c_str(), 0, 100, conf_.width,
                               conf_.height, flags);
}

VideoRenderer::~VideoRenderer() { SDL_DestroyWindow(window_); }

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
#ifdef __linux__
    return info.info.x11.window;
#elif defined _WIN32
    return reinterpret_cast<intptr_t>(info.info.win.window);
#endif
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
        update_textures(yuv->DataY(), yuv->DataU(), yuv->DataV());
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
