#include "video_renderer.hh"
#include "logger.hh"
#include "opengl_renderer.hh"
#include "sdl_renderer.hh"

#include <cstdio>
#include <filesystem>
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
    uint32_t flags = SDL_WINDOW_OPENGL; // | SDL_WINDOW_ALLOW_HIGHDPI;
    if (conf_.hide)
        flags |= SDL_WINDOW_HIDDEN;

    window_ = SDL_CreateWindow(conf_.name.c_str(), 0, 100, conf_.width,
                               conf_.height, flags);
}

VideoRenderer::~VideoRenderer()
{
    Stop();
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
    dump_frame(last_frame_, 0);
}

/*
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
*/

// running on capture thread (local) or worker thread (remote)?
void VideoRenderer::OnFrame(const webrtc::VideoFrame &frame)
{
    static int once = 0;
    if (conf_.dump && once < 1200 && once % 60 == 0) {
        dump_frame(frame.video_frame_buffer(), once);
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
        last_frame_ = frame;
    }
}

// TODO: encode as jpeg?
void VideoRenderer::dump_frame(FramePtr frame, int id)
{
    auto yuv = frame->GetI420();
    std::string name = fmt::format("frame-{:2}.yuv", id);
    ::FILE *f = ::fopen(name.c_str(), "wb+");
    fwrite(yuv->DataY(), 1, yuv->StrideY() * yuv->height(), f);
    fwrite(yuv->DataU(), 1, yuv->StrideU() * yuv->height() / 2, f);
    fwrite(yuv->DataV(), 1, yuv->StrideV() * yuv->height() / 2, f);
    fflush(f);
    fclose(f);
}
