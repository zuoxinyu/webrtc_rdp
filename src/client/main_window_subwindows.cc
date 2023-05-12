#include "main_window.hh"

extern "C" {
#include "ui/microui.h"
#include "ui/renderer.h"
}

void MainWindow::stats_window(mu_Context *ctx)
{
    if (mu_begin_window(ctx, "Stats", mu_rect(400, 0, 800, 800))) {
        mu_layout_row(ctx, 1, std::vector<int>{-1}.data(), -25);
        mu_begin_panel(ctx, "Stats Panel");
        mu_Container *panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, std::vector<int>{-1}.data(), -1);
        mu_text(ctx, stats_json_.c_str());
        mu_end_panel(ctx);
        mu_end_window(ctx);
    }
}

void MainWindow::chat_window(mu_Context *ctx)
{
    std::string title = "Chat vs " + cc_->peer().name;

    if (mu_begin_window(ctx, title.c_str(), mu_rect(400, 0, 800, 800))) {
        mu_layout_row(ctx, 1, std::vector<int>{-1}.data(), -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container *panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, std::vector<int>{-1}.data(), -1);
        mu_text(ctx, chatbuf_.data());
        mu_end_panel(ctx);
        if (chatbuf_updated_) {
            panel->scroll.y = panel->content_size.y;
            chatbuf_updated_ = false;
        }

        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        mu_layout_row(ctx, 2, std::vector<int>{-70, -1}.data(), 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Send")) {
            submitted = 1;
        }
        if (submitted) {
            update_chat(cc_->name(), buf);
            post_chat(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}

void MainWindow::peers_window(mu_Context *ctx)
{
    auto peer_item = [this](mu_Context *ctx, const Peer &p) {
        mu_layout_row(ctx, 6, std::vector<int>{80, 80, 50, 50, 50, 50}.data(),
                      25);
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
            mu_layout_row(ctx, 3, std::vector<int>{80, 80, -1}.data(), 30);
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
            mu_layout_row(ctx, 2, std::vector<int>{150, 150}.data(), 25);
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

            mu_layout_row(ctx, 1, std::vector<int>{-1}.data(), 25);
            if (mu_button(ctx, "Login")) {
                submit = 1;
            }

            if (submit || auto_login_) {
                login();
            }
        }

        mu_layout_height(ctx, -1);
        mu_layout_row(ctx, 1, std::vector<int>{-1}.data(), 25);
        if (mu_button(ctx, "Exit")) {
            stop();
        }

        mu_end_window(ctx);
    }
}
