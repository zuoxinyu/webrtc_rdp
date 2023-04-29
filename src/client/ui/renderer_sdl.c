#ifdef USE_SDL_RENDERER
#include "renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

const char button_map[256] = {
    [SDL_BUTTON_LEFT & 0xff] = MU_MOUSE_LEFT,
    [SDL_BUTTON_RIGHT & 0xff] = MU_MOUSE_RIGHT,
    [SDL_BUTTON_MIDDLE & 0xff] = MU_MOUSE_MIDDLE,
};

const char key_map[256] = {
    [SDLK_LSHIFT & 0xff] = MU_KEY_SHIFT,
    [SDLK_RSHIFT & 0xff] = MU_KEY_SHIFT,
    [SDLK_LCTRL & 0xff] = MU_KEY_CTRL,
    [SDLK_RCTRL & 0xff] = MU_KEY_CTRL,
    [SDLK_LALT & 0xff] = MU_KEY_ALT,
    [SDLK_RALT & 0xff] = MU_KEY_ALT,
    [SDLK_RETURN & 0xff] = MU_KEY_RETURN,
    [SDLK_BACKSPACE & 0xff] = MU_KEY_BACKSPACE,
};

float bg[3] = {90, 95, 100};

static int kWidth = 1920;
static int kHeight = 1200;
static int kFontSize = 12;
static const char *kDefaultFont =
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc";
static const char *kIconFont =
    "/usr/share/fonts/TTF/Fira Code Regular Nerd Font Complete.ttf";

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static TTF_Font *font = NULL;
static TTF_Font *icon_font = NULL;

SDL_Window *r_get_window() { return window; }

void r_init()
{
    window =
        SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         kWidth, kHeight, SDL_WINDOW_OPENGL);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_TARGET, kWidth, kHeight);

    font = TTF_OpenFont(kDefaultFont, kFontSize);
    if (!font) {
        fprintf(stderr, "failed to open font: %s, reason: %s\n", kDefaultFont,
                TTF_GetError());
        TTF_GetError();
        exit(-1);
    }
    icon_font = TTF_OpenFont(kIconFont, 18);
    if (!font) {
        fprintf(stderr, "failed to open font: %s, reason: %s\n", kDefaultFont,
                TTF_GetError());
        TTF_GetError();
        exit(-1);
    }
}

static void flush() { SDL_RenderFlush(renderer); }

void r_draw_rect(mu_Rect rect, mu_Color color)
{
    SDL_Rect r = {rect.x, rect.y, rect.w, rect.h};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &r);
    SDL_RenderFillRect(renderer, &r);
}

static void r_draw_text_with_font(const char *text, TTF_Font *fnt, mu_Rect rect,
                                  mu_Color color)
{
    if (!text || !*text) {
        return;
    }

    SDL_Color c = {color.r, color.g, color.b, color.a};
    SDL_Surface *text_surface = TTF_RenderUTF8_Blended(fnt, text, c);
    SDL_Texture *text_texture =
        SDL_CreateTextureFromSurface(renderer, text_surface);
    int w = text_surface->clip_rect.w;
    int h = text_surface->clip_rect.h;
    int xoffset = rect.w > w ? (rect.w - w) / 2 : 0;
    int yoffset = rect.h > h ? (rect.h - h) / 2 : 0;
    SDL_Rect r = {rect.x + xoffset, rect.y + yoffset, w, h};

    SDL_RenderCopy(renderer, text_texture, NULL, &r);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color)
{
    r_draw_text_with_font(text, font, (mu_Rect){pos.x, pos.y, 0, 0}, color);
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color)
{
    switch (id) {
    case MU_ICON_CLOSE:
        r_draw_text_with_font("", icon_font, rect, color);
        break;
    case MU_ICON_CHECK:
        r_draw_text_with_font("", icon_font, rect, color);
        break;
    case MU_ICON_EXPANDED:
        r_draw_text_with_font("", icon_font, rect, color);
        break;
    case MU_ICON_COLLAPSED:
        r_draw_text_with_font("", icon_font, rect, color);
        break;
    }
}

int r_get_text_width(const char *text, int len)
{
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) {
            continue;
        }
        /*int chr =mu_min((unsigned char)*p, 127)*/;
        res += 6;
    }
    return res;
}

int r_get_text_height() { return 18; }

void r_set_clip_rect(mu_Rect rect)
{
    flush();
    SDL_Rect r = {rect.x, rect.y, rect.w, rect.h};
    SDL_RenderSetClipRect(renderer, &r);
}

void r_clear(mu_Color color)
{
    flush();

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);
    // SDL_RenderCopy(renderer, texture, NULL, NULL);
}

void r_present()
{
    flush();
    SDL_RenderPresent(renderer);
}

void push_render_target()
{
    texture = SDL_GetRenderTarget(renderer);
    int ret = SDL_SetRenderTarget(renderer, texture);
    if (ret < 0) {
        fprintf(stderr, "SDL_SetRenderTarget: %s\n", SDL_GetError());
    }
}

void pop_render_target()
{
    int ret = SDL_SetRenderTarget(renderer, texture);
    if (ret < 0) {
        fprintf(stderr, "SDL_SetRenderTarget: %s\n", SDL_GetError());
    }
}

#endif
