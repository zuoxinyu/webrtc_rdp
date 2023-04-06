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

#include "main_window.hh"

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
    mu_Context *ctx = init();

    std::cout << "hello chatz!\n";
    MainWindow wnd(ctx);
    wnd.run();
}
