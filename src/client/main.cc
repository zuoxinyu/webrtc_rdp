#include "logger.hh"
#include "main_window.hh"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

extern "C" {
#include "microui.h"
#include "renderer.h"
#include <SDL2/SDL.h>
}

#include <spdlog/spdlog.h>

static int text_width(mu_Font font, const char *text, int len)
{
    if (len == -1) {
        len = strlen(text);
    }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) { return r_get_text_height(); }

static mu_Context *init()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();
    mu_Context *ctx = new mu_Context;
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    return ctx;
}

int main(int argc, char *argv[])
{
    std::string title = "my_name";
    if (argc > 1) {
        title = argv[1];
    }
    mu_Context *ctx = init();

    spdlog::set_level(spdlog::level::debug);
    logger::info("Hello dezk");
    MainWindow wnd(ctx, title);
    wnd.run();
}
