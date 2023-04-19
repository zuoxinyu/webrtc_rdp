#include "main_window.hh"
#include "event_executor.hh"

#include <cstdlib>
#include <cstring>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_video.h>

extern "C" {
#include "ui/microui.h"
#include "ui/renderer.h"
}

static constexpr auto kWidth = 1920;
static constexpr auto kHeight = 1200;
static const VideoRenderer::Config local_opts = {.name = "local camera video",
                                                 .width = 600,
                                                 .height = 400,
                                                 .use_opengl = false,
                                                 .dump = false,
                                                 .hide = true};
static const VideoRenderer::Config remote_opts = {.name =
                                                      "remote desktop video",
                                                  .width = kWidth,
                                                  .height = kHeight,
                                                  .use_opengl = false,
                                                  .dump = false,
                                                  .hide = true};

static char hostbuf[16] = "127.0.0.1";
static char portbuf[8] = "8888";
static char namebuf[16] = "user";

void parseEnvs()
{
    if (auto name = ::getenv("SIGNAL_DEFAULT_NAME")) {
        std::strcpy(namebuf, name);
    }
    if (auto host = ::getenv("SIGNAL_DEFAULT_HOST")) {
        std::strcpy(hostbuf, host);
    }
    if (auto port = ::getenv("SIGNAL_DEFAULT_PORT")) {
        std::strcpy(portbuf, port);
    }
}

MainWindow::MainWindow(mu_Context *ctx, int argc, char *argv[])
    : ioctx_(), ctx_(ctx)
{
    parseEnvs();
    /* absl::ParseCommandLine(argc, argv); */

    auto_login_ = argc > 1 && argv[1];

    pc_ = make_unique<PeerClient>();
    cc_ = std::make_unique<SignalClient>(ioctx_);
    local_renderer_ = VideoRenderer::Create(local_opts);
    remote_renderer_ = VideoRenderer::Create(remote_opts);
    remote_video_src_ = ScreenCapturer::Create({});

    executor_ = EventExecutor::create(remote_renderer_->get_window());
    stats_observer_ = StatsObserver::Create(stats_json_);

    cc_->set_peer_observer(pc_.get()); // recursive reference?
    pc_->set_signaling_observer(cc_.get());
    pc_->set_stats_observer(stats_observer_.get());
    pc_->add_screen_video_source(remote_video_src_);
    pc_->add_screen_sinks(remote_renderer_);
    if (CameraCapturer::GetDeviceNum() > 0) {
        local_video_src_ =
            CameraCapturer::Create({.width = 600, .height = 400, .fps = 30});
        pc_->add_camera_video_source(local_video_src_);
        pc_->add_camera_sinks(local_renderer_);
    }
}

void MainWindow::login()
{
    auto_login_ = false;

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
    if (chatbuf_[0]) {
        std::strcat(chatbuf_, "\n");
    }
    auto prefix = who + ": ";
    std::strcat(chatbuf_, prefix.c_str());
    std::strcat(chatbuf_, buf);
    chatbuf_updated_ = true;
}
void MainWindow::open_chat()
{
    if (cc_->calling())
        show_stats_ = false;
}

void MainWindow::stop() { running_ = false; }

void MainWindow::run()
{
    running_ = true;

    auto is_main_event = [](SDL_Event &e) {
        return e.window.windowID == SDL_GetWindowID(r_get_window());
    };

    auto is_remote_event = [this](SDL_Event &e) {
        return remote_renderer_ &&
               e.window.windowID ==
                   SDL_GetWindowID(remote_renderer_->get_window());
    };

    SDL_Event e;
    EventExecutor::Event ee;
    std::optional<PeerClient::ChanMessage> msg;

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

    while (running_) {
        ioctx_.restart();
        while (!ioctx_.stopped() && ioctx_.poll()) {
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
            }

            if (is_main_event(e)) {
                handle_main_event(e);
            } else if (is_remote_event(e)) {
                handle_remote_event(e);
            }
        }

        process_mu_windows();

        process_mu_commands();

        ::usleep(12000);
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

void MainWindow::stats_window(mu_Context *ctx)
{
    if (mu_begin_window(ctx, "Stats", mu_rect(400, 0, 800, 800))) {
        mu_layout_row(ctx, 1, (int[]){-1}, -25);
        mu_begin_panel(ctx, "Stats Panel");
        mu_Container *panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, (int[]){-1}, -1);
        mu_text(ctx, stats_json_.c_str());
        mu_end_panel(ctx);
        mu_end_window(ctx);
    }
}

void MainWindow::chat_window(mu_Context *ctx)
{
    std::string title = "Chat vs " + cc_->peer().name;

    if (mu_begin_window(ctx, title.c_str(), mu_rect(400, 0, 800, 800))) {
        mu_layout_row(ctx, 1, (int[]){-1}, -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container *panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, (int[]){-1}, -1);
        mu_text(ctx, chatbuf_);
        mu_end_panel(ctx);
        if (chatbuf_updated_) {
            panel->scroll.y = panel->content_size.y;
            chatbuf_updated_ = false;
        }

        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        mu_layout_row(ctx, 2, (int[]){-70, -1}, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Send")) {
            submitted = 1;
        }
        if (submitted) {
            update_chat(cc_->name(), buf);
            pc_->post_text_message(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}

void MainWindow::peers_window(mu_Context *ctx)
{
    auto peer_item = [this](mu_Context *ctx, const Peer &p) {
        mu_layout_row(ctx, 6, (int[]){80, 80, 50, 50, 50, 50}, 25);
        mu_label(ctx, p.name.c_str());
        mu_label(ctx, p.id.c_str());

        if (p.id != cc_->id()) {
            if (mu_button_ex(ctx, nullptr, MU_ICON_CHECK, 0)) {
                connect(p.id);
            }
            if (mu_button_ex(ctx, nullptr, MU_ICON_CLOSE, 0)) {
                disconnect();
            }
            if (mu_button(ctx, "chat")) {
                open_chat();
            }
            if (mu_button(ctx, "stats")) {
                open_stats();
            }
        } else {
            mu_label(ctx, "");
            mu_label(ctx, "");
            mu_label(ctx, "");
            mu_label(ctx, "");
        }
    };

    if (mu_begin_window(ctx, "Online Peers", mu_rect(0, 0, 400, 600))) {
        if (mu_header_ex(ctx, "Online Peers", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 3, (int[]){80, 80, -1}, 30);
            mu_label(ctx, "Name");
            mu_label(ctx, "ID");
            mu_label(ctx, "Actions");

            for (auto &[id, p] : cc_->peers()) {
                peer_item(ctx, p);
            }
        }
        mu_end_window(ctx);
    }
}

void MainWindow::login_window(mu_Context *ctx)
{
    static int voice_enabled = 0;
    static int camera_enabled = 0;

    if (mu_begin_window(ctx, "Login", mu_rect(0, 600, 400, 200))) {
        {
            mu_layout_row(ctx, 2, (int[]){150, 150}, 25);
            if (mu_checkbox(ctx, "voice", &voice_enabled)) {
            }

            if (mu_checkbox(ctx, "camera", &camera_enabled)) {
            }
        }
        if (!cc_->online()) {
            int ws[2] = {-240, -1};
            int submit = 0;
            mu_layout_row(ctx, 2, ws, 25);
            mu_text(ctx, "Name:");
            if (mu_textbox(ctx, namebuf, sizeof(namebuf)) & MU_RES_SUBMIT) {
                mu_set_focus(ctx, ctx->last_id);
                submit = 1;
            }
            mu_text(ctx, "Host:");
            if (mu_textbox(ctx, hostbuf, sizeof(hostbuf)) & MU_RES_SUBMIT) {
                mu_set_focus(ctx, ctx->last_id);
                submit = 1;
            }
            mu_text(ctx, "Port:");
            if (mu_textbox(ctx, portbuf, sizeof(portbuf)) & MU_RES_SUBMIT) {
                mu_set_focus(ctx, ctx->last_id);
                submit = 1;
            }

            mu_layout_row(ctx, 1, (int[]){-1}, 25);
            if (mu_button(ctx, "Login")) {
                submit = 1;
            }

            if (submit || auto_login_) {
                login();
            }
        }

        mu_layout_height(ctx, -1);
        mu_layout_row(ctx, 1, (int[]){-1}, 25);
        if (mu_button(ctx, "Exit")) {
            stop();
        }

        mu_end_window(ctx);
    }
}

void MainWindow::handle_main_event(const SDL_Event &e)
{
    switch (e.type) {
    case SDL_WINDOWEVENT:
        switch (e.window.type) {
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
    }
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

void MainWindow::handle_remote_event(const SDL_Event &e)
{
    SDL_Event ev = e;
    // TODO: handle failure
    pc_->post_binary_message(reinterpret_cast<const uint8_t *>(&ev),
                             sizeof(ev));
}
