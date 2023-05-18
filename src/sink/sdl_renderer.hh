#pragma once
#include "video_renderer.hh"

struct SDLRenderer : public VideoRenderer {
  public:
    SDLRenderer(Config conf);
    ~SDLRenderer() override;

  private:
    void update_textures(const void *ydata, const void *udata,
                         const void *vdata) override;

  private:
    // resources
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
};
