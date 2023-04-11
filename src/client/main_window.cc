#include "main_window.hh"

extern "C" {
#include "microui.h"
#include "renderer.h"
#include <SDL2/SDL.h>
}

MainWindow::MainWindow(mu_Context *ctx, const std::string &title)
    : title_(title), io_ctx_(), ctx_(ctx),
      cc_(std::make_unique<SignalClient>(io_ctx_, title)),
      pc_(rtc::make_ref_counted<PeerClient>()),
      local_renderer_(std::make_unique<VideoRenderer>()),
      remote_renderer_(std::make_unique<VideoRenderer>())
{
    pc_->setSignalingObserver(cc_.get());
    cc_->setPeerObserver(pc_.get());
    if (pc_->createPeerConnection()) {
        pc_->addLocalSinks(local_renderer_.get());
        pc_->addRemoteSinks(remote_renderer_.get());
        pc_->createLocalTracks();
    }
}

void MainWindow::run()
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
            case SDL_WINDOWEVENT:
                switch (e.window.type) {
                case SDL_WINDOWEVENT_CLOSE:
                    SDL_DestroyWindow(SDL_GetWindowFromID(e.window.windowID));
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

        /* process frame */
        render_windows(ctx_);

        /* render */
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
        /* renderer_->update_frame(); */
    }
}

void MainWindow::render_windows(mu_Context *ctx)
{
    mu_begin(ctx);
    if (cc_->online()) {
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
            int ws[3] = {-80, -40, -1};
            mu_layout_row(ctx, 3, ws, 30);
            for (auto &pair : cc_->peers()) {
                mu_label(ctx, pair.second.name.c_str());
                mu_label(ctx, pair.second.id.c_str());
                std::string btn_id = pair.second.id;
                if (mu_button(ctx, btn_id.c_str())) {
                    // start calling
                    cc_->setCurrentPeer(pair.second.id);
                    pc_->makeCall();
                }
            }
        }
        if (mu_header_ex(ctx, "Offline Peers", MU_OPT_EXPANDED)) {
            int ws[2] = {-80, -1};
            mu_layout_row(ctx, 2, ws, 30);
            for (auto &pair : cc_->offlinePeers()) {
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
        int ws[2] = { -240, -1 };
        mu_layout_row(ctx, 2, ws, 25);
        int submit = 0;
        static char hostbuf[16] = "127.0.0.1";
        static char portbuf[8] = "8888";
        mu_text(ctx, "Host:");
        if (mu_textbox(ctx, hostbuf, sizeof(hostbuf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submit = 1;
        }
        mu_layout_row(ctx, 2, ws, 25);
        mu_text(ctx, "Port:");
        if (mu_textbox(ctx, portbuf, sizeof(portbuf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submit = 1;
        }
        if (mu_button(ctx, "Login")) {
            submit = 1;
        }

        if (submit) {
            int port = std::atoi(portbuf);
            std::string host{hostbuf};
            co_spawn(io_ctx_, cc_->signin(host, port), detached);
        }

        mu_end_window(ctx);
    }
}
