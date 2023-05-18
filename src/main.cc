#include "SDL2/SDL_video.h"
#include "main_window.hh"
#include <slint.h>
extern "C" {
#include "ui/microui.h"
#include "ui/renderer.h"
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_ttf.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <fmt/format.h>

#include "rtc_base/logging.h"

ABSL_FLAG(bool, rtclog, false, "enable webrtc debug level logging");
ABSL_FLAG(bool, debug, false, "enable verbose logging");

int main(int argc, char *argv[])
{
    absl::SetProgramUsageMessage(fmt::format("sample usage: {}", argv[0]));
    absl::ParseCommandLine(argc, argv);

    rtc::LogMessage::LogToDebug(absl::GetFlag(FLAGS_rtclog) ? rtc::LS_INFO
                                                            : rtc::LS_WARNING);
    logger::set_level(absl::GetFlag(FLAGS_debug) ? spdlog::level::trace
                                                 : spdlog::level::debug);
    logger::info("Hello dezk");

    if (SDL_Init(SDL_INIT_EVERYTHING) || TTF_Init()) {
        logger::critical("failed to init SDL or SDL_TTF: {}", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
    SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");

    MainWindow wnd(argc, argv);
    wnd.run();
    return 0;
}
