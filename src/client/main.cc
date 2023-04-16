#include "logger.hh"
#include "main_window.hh"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "rtc_base/logging.h"
#include <spdlog/spdlog.h>

extern "C" {
#include "microui.h"
#include "renderer.h"
#include <SDL2/SDL.h>
}

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
    auto *ctx = new mu_Context;
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    return ctx;
}

#ifdef WIN32
#undef SDL_main
#endif

static const char *KDefaultName = "DEFAULT_SIGNAL_NAME";
static const char *kDefaultServer = "DEFAULT_SIGNAL_SERVER";
static const char *kDefaultPort = "DEFAULT_SIGNAL_PORT";

int main(int argc, char *argv[])
{
    rtc::LogMessage::LogToDebug(rtc::LS_ERROR);
    spdlog::set_level(spdlog::level::debug);
    logger::info("Hello dezk");

    mu_Context *ctx = init();
    MainWindow wnd(ctx, argc, argv);
    wnd.run();
    return 0;
}
