#include "video_renderer.hh"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include "api/video/i420_buffer.h"
using namespace std::chrono_literals;

void init()
{
    assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
    // assert(glewInit() == 0);
}

int main()
{
    init();

    const int w = 2560, h = 1600;
    VideoRenderer::Config conf = {
        .width = w,
        .height = h,
        .use_opengl = true,
    };
    auto player = VideoRenderer::Create(conf);

    player->Start();

    FILE *yuv = fopen("frame-00.yuv", "rb");

    webrtc::VideoFrame::Builder builder;
    auto buffer = webrtc::I420Buffer::Create(w, h);

    fread(buffer->MutableDataY(), w * h, 1, yuv);
    fread(buffer->MutableDataU(), w * h / 4, 1, yuv);
    fread(buffer->MutableDataV(), w * h / 4, 1, yuv);

    builder.set_video_frame_buffer(buffer);

    player->OnFrame(builder.build());

    SDL_Event e;
    bool run = true;
    while (run) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                run = false;
            }
        }
        player->update_frame();
        std::this_thread::sleep_for(100ms);
    }

    fclose(yuv);
}
