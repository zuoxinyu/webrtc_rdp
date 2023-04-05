#include "main_window.hh"

extern "C" {
#include "renderer.h"
#include <SDL2/SDL.h>
#include <microui.h>
}

MainWindow::MainWindow(mu_Context *ctx)
    : io_ctx_(), ctx_(ctx), thread_(nullptr),
      client_(std::make_unique<ChatClient>(io_ctx_)),
      renderer_(std::make_unique<VideoRenderer>(r_get_window())),
      pc_(rtc::make_ref_counted<PeerConnectionImpl>())
{
    if (pc_->createPeerConnection()) {
        pc_->addSinks(renderer_.get());
        pc_->addTracks();
    }
    // thread_ = std::make_unique<std::thread>([this] { io_ctx_.run(); });
}

void MainWindow::Run()
{
    while (true) {
        if (io_ctx_.stopped()) {
            io_ctx_.restart();
        }
        io_ctx_.poll();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                std::exit(EXIT_SUCCESS);
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

        /* process frame */
        render(ctx_);

        /* render */
        r_clear(mu_color(bg[0], bg[1], bg[2], 255));
        mu_Command *cmd = NULL;
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
}

void MainWindow::render(mu_Context *ctx)
{
    mu_begin(ctx);
    if (client_->is_signed()) {
        peers_window(ctx);
    } else {
        login_window(ctx);
    }

    mu_end(ctx);
}

void MainWindow::peers_window(mu_Context *ctx)
{
    if (mu_begin_window(ctx, "Online Peers", mu_rect(0, 40, 300, 200))) {
        if (mu_header_ex(ctx, "Online Peers", MU_OPT_EXPANDED)) {
            int ws[2] = {-80, -1};
            mu_layout_row(ctx, 2, ws, 30);
            for (auto &pair : client_->online_peers()) {
                mu_label(ctx, pair.second.name.c_str());
                mu_label(ctx, pair.second.id.c_str());
            }
        }
        if (mu_header_ex(ctx, "Offline Peers", MU_OPT_EXPANDED)) {
            int ws[2] = {-80, -1};
            mu_layout_row(ctx, 2, ws, 30);
            for (auto &pair : client_->offline_peers()) {
                mu_label(ctx, pair.second.name.c_str());
                mu_label(ctx, pair.second.id.c_str());
            }
        }
        mu_end_window(ctx);
    }
}

void MainWindow::login_window(mu_Context *ctx)
{
    if (mu_begin_window(ctx, "Login", mu_rect(0, 40, 300, 200))) {
        mu_layout_row(ctx, 2, std::array{-240, -1}.begin(), 25);
        int submit = 0;
        static char hostbuf[16] = "127.0.0.1";
        static char portbuf[8] = "8888";
        mu_text(ctx, "Host:");
        if (mu_textbox(ctx, hostbuf, sizeof(hostbuf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submit = 1;
        }
        mu_layout_row(ctx, 2, std::array{-240, -1}.begin(), 25);
        mu_text(ctx, "Port:");
        if (mu_textbox(ctx, portbuf, sizeof(portbuf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submit = 1;
        }
        if (mu_button(ctx, "Login")) {
            submit = 1;
        }

        if (submit) {
            int port;
            sscanf(portbuf, "%d", &port);
            std::string host{hostbuf};
            co_spawn(io_ctx_, client_->StartLogin(host, port), detached);
        }

        mu_end_window(ctx);
    }
}
