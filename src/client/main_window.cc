#include "main_window.hh"
#include "event_executor.hh"
#include "ui/sdl_trigger.hh"

#include <cstdlib>
#include <cstring>
#include <thread>

#include <absl/flags/flag.h>
#include <boost/asio.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_video.h>

extern "C" {
#include "ui/microui.h"
#include "ui/renderer.h"
}

static const VideoRenderer::Config camera_opts = {.name = "camera video",
                                                  .width = 600,
                                                  .height = 400,
                                                  .use_opengl = false,
                                                  .dump = false,
                                                  .hide = true};
static const VideoRenderer::Config desktop_opts = {.name = "remote desktop",
                                                   .width = 1920,
                                                   .height = 1200,
                                                   .use_opengl = true,
                                                   .dump = false,
                                                   .hide = true};
static const ScreenCapturer::Config capture_opts = {.fps = 60,
                                                    .width = 2560,
                                                    .height = 1600,
                                                    .keep_ratio = true,
                                                    .exlude_window_id = {}};
static const CameraCapturer::Config cameracap_opts = {
    .width = 600, .height = 400, .fps = 30, .uniq = ""};
ABSL_FLAG(std::string, user, "username", "signaling name");
ABSL_FLAG(std::string, host, "10.10.10.133", "signal server host");
ABSL_FLAG(int, port, 8888, "signal server port");
ABSL_FLAG(bool, auto_login, true, "auto login");
ABSL_FLAG(std::vector<std::string>, servers, {"stun:10.10.10.133"},
          "stun servers");
ABSL_FLAG(bool, use_opengl, true, "use OpenGL instead of SDL2");
ABSL_FLAG(bool, use_h264, false, "use custom H264 codec implementation");

MainWindow::MainWindow(mu_Context *ctx, int argc, char *argv[]) : ctx_(ctx)
{
    need_login_ = absl::GetFlag(FLAGS_auto_login);

    chatbuf_.reserve(6400);
    pc_conf_.stun_servers = absl::GetFlag(FLAGS_servers);
    pc_conf_.use_codec = absl::GetFlag(FLAGS_use_h264);
    cc_conf_.host = absl::GetFlag(FLAGS_host);
    cc_conf_.port = absl::GetFlag(FLAGS_port);
    cc_conf_.name = absl::GetFlag(FLAGS_user);
    std::strcpy(namebuf, absl::GetFlag(FLAGS_user).c_str());
    std::strcpy(hostbuf, absl::GetFlag(FLAGS_host).c_str());
    std::strcpy(portbuf, std::to_string(absl::GetFlag(FLAGS_port)).c_str());

    // TODO: delay heavy works
    pc_ = std::make_unique<PeerClient>(pc_conf_);
    cc_ = std::make_unique<SignalClient>(ioctx_, cc_conf_);
    screen_renderer_ = VideoRenderer::Create(desktop_opts);
    screen_video_src_ = ScreenCapturer::Create(capture_opts);

    executor_ = EventExecutor::create(screen_renderer_->get_window());
    stats_observer_ = StatsObserver::Create(stats_json_);

    cc_->set_peer_observer(pc_.get()); // recursive reference?
    pc_->set_signaling_observer(cc_.get());
    pc_->set_stats_observer(stats_observer_.get());
    pc_->add_screen_video_source(screen_video_src_);
    pc_->add_screen_sinks(screen_renderer_);
    auto cameras = CameraCapturer::GetDeviceList();
    if (!cameras.empty()) {
        logger::info("supported cameras: {}", cameras);
        auto opts = cameracap_opts;
        opts.uniq = cameras[0].second.c_str();
        camera_renderer_ = VideoRenderer::Create(camera_opts);
        camera_video_src_ = CameraCapturer::Create(opts);
        pc_->add_camera_video_source(camera_video_src_);
        pc_->add_camera_sinks(camera_renderer_);
    }
}

void MainWindow::login()
{
    need_login_ = false;

    auto port = std::atoi(portbuf);
    auto host = std::string{hostbuf};

    cc_->set_name(namebuf);
    cc_->login(host, port);
}

void MainWindow::logout()
{
    disconnect();
    cc_->logout();
}

void MainWindow::connect(const Peer::Id &id)
{
    if (cc_->calling()) {
        disconnect();
    }
    cc_->start_session(id);
}

void MainWindow::disconnect()
{
    if (cc_->calling())
        cc_->stop_session();
}

void MainWindow::open_stats()
{
    if (cc_->calling()) {
        show_stats_ = true;
        pc_->get_stats();
    }
}

void MainWindow::update_chat(const std::string &who, const char *buf)
{
    fmt::format_to(std::back_inserter(chatbuf_), "{}: {}\n", who, buf);
    chatbuf_.data()[chatbuf_.size()] = 0;
    chatbuf_updated_ = true;
}

void MainWindow::post_chat(const std::string &msg)
{
    pc_->post_text_message(msg);
}

void MainWindow::open_chat()
{
    if (cc_->calling())
        show_stats_ = false;
}

void MainWindow::stop() { running_ = false; }

void MainWindow::run()
{
    auto main_id = SDL_GetWindowID(r_get_window());
    auto desk_id = SDL_GetWindowID(screen_renderer_->get_window());

    auto is_main_event = [main_id](SDL_Event &e) -> bool {
        return e.window.windowID == main_id || e.type == SDL_CLIPBOARDUPDATE;
    };

    auto is_remote_event = [this, desk_id](SDL_Event &e) -> bool {
        return screen_renderer_ &&
               (e.window.windowID == desk_id || e.drop.windowID == desk_id);
    };

    SDL_Event e;
    EventExecutor::Event ee;
    std::optional<PeerClient::ChanMessage> msg;
    auto work = boost::asio::make_work_guard(ioctx_);

    auto timer = SDL_AddTimer(
        5000,
        [](unsigned tick, void *param) -> unsigned {
            auto *that = static_cast<MainWindow *>(param);
            if (that->cc_->calling()) {
                that->pc_->get_stats();
            }
            return tick;
        },
        this);
    Trigger::on({SDLK_LCTRL, SDLK_LSHIFT, SDLK_LALT, SDLK_q}, [this] {
        auto window = screen_renderer_->get_window();
        auto state = SDL_GetWindowGrab(window);
        SDL_SetWindowGrab(window, state ? SDL_FALSE : SDL_TRUE);
        logger::debug("escape shortcuts met, {} grab mode",
                      state ? "leaving" : "entering");
    });

    running_ = true;
    while (running_) {
        while (ioctx_.poll()) {
            ;
        }

        while (pc_ && (msg = pc_->poll_remote_message())) {
            if (msg->binary) {
                ee.native_ev = *reinterpret_cast<SDL_Event *>(
                    const_cast<uint8_t *>(msg.value().data));

                executor_->execute(ee);
            } else {
                std::string text((const char *)msg->data, msg->size);
                update_chat(cc_->peer().name, text.c_str());
            }
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                stop();
            } else if (is_main_event(e)) {
                handle_main_event(e);
            } else if (is_remote_event(e)) {
                handle_remote_event(e);
            }
        }

        process_mu_windows();

        process_mu_commands();

        screen_renderer_->update_frame();
        //::usleep(12000);
    }

    SDL_RemoveTimer(timer);
}

void MainWindow::process_mu_windows()
{
    mu_begin(ctx_);
    peers_window(ctx_);
    login_window(ctx_);
    if (cc_->calling()) {
        if (show_stats_) {
            stats_window(ctx_);
        } else {
            chat_window(ctx_);
        }
    }
    mu_end(ctx_);
}

void MainWindow::handle_main_event(SDL_Event &e)
{
    switch (e.type) {
    case SDL_QUIT:
        stop();
        break;
    case SDL_WINDOWEVENT:
        switch (e.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            stop();
            break;
        }
        break;
    case SDL_MOUSEMOTION:
        mu_input_mousemove(ctx_, e.motion.x, e.motion.y);
        break;
    case SDL_MOUSEWHEEL:
        mu_input_scroll(ctx_, 0, e.wheel.y * -30);
        break;
    case SDL_TEXTINPUT:
        mu_input_text(ctx_, e.text.text);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        int b = button_map[e.button.button & 0xff];
        if (b && e.type == SDL_MOUSEBUTTONDOWN) {
            mu_input_mousedown(ctx_, e.button.x, e.button.y, b);
        }
        if (b && e.type == SDL_MOUSEBUTTONUP) {
            mu_input_mouseup(ctx_, e.button.x, e.button.y, b);
        }
        break;
    }

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        int c = key_map[e.key.keysym.sym & 0xff];
        if (c && e.type == SDL_KEYDOWN) {
            mu_input_keydown(ctx_, c);
        }
        if (c && e.type == SDL_KEYUP) {
            mu_input_keyup(ctx_, c);
        }
        break;
    }
    case SDL_CLIPBOARDUPDATE:
        char *clip_text = SDL_GetClipboardText();
        logger::debug("clip board updated: {}", clip_text);
        SDL_free(clip_text);
        break;
    }
}

void MainWindow::handle_remote_event(SDL_Event &e)
{
    Trigger::processEvent(e);
    SDL_Window *window = screen_renderer_->get_window();
    switch (e.type) {
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
            disconnect();
            return;
        }
        break;
    case SDL_DROPBEGIN:
    case SDL_DROPCOMPLETE:
        logger::debug("begin or end dropping: {}", e.drop.type);
        break;
    case SDL_DROPFILE: {
        char *file_name = e.drop.file;
        logger::debug("transferring file: {}", file_name);
        SDL_free(file_name);
    } break;
    default: {
        SDL_Event ev = e;
        pc_->post_binary_message(reinterpret_cast<const uint8_t *>(&ev),
                                 sizeof(ev));
    } break;
    }
    // TODO: handle failure
}

void MainWindow::process_mu_commands()
{
    r_clear(mu_color(bg[0], bg[1], bg[2], 255));
    mu_Command *cmd = nullptr;
    while (mu_next_command(ctx_, &cmd)) {
        switch (cmd->type) {
        case MU_COMMAND_TEXT:
            r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
            break;
        case MU_COMMAND_RECT:
            r_draw_rect(cmd->rect.rect, cmd->rect.color);
            break;
        case MU_COMMAND_ICON:
            r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            break;
        case MU_COMMAND_CLIP:
            r_set_clip_rect(cmd->clip.rect);
            break;
        }
    }
    r_present();
}
