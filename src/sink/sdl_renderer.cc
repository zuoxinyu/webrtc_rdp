#include "sdl_renderer.hh"
#include "logger.hh"

#include <cstdio>
#include <stdexcept>

#include <SDL2/SDL_render.h>

SDLRenderer::SDLRenderer(Config conf) : VideoRenderer(std::move(conf))
{
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV,
                                 SDL_TEXTUREACCESS_STREAMING, conf_.width,
                                 conf_.height);
    SDL_SetRenderDrawColor(renderer_, 0, 128, 128, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);
}

SDLRenderer::~SDLRenderer()
{
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
}

void SDLRenderer::update_textures(const void *ydata, const void *udata,
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
