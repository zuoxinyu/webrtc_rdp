#include "main_window.hh"
#include "executor/event_executor.hh"
#include "ui/sdl_trigger.hh"
#include <slint_sharedvector.h>
extern "C" {
#include "ui/microui.h"
#include "ui/renderer.h"
}

#include <chrono>
#include <functional>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_video.h>
#include <absl/flags/flag.h>
#include <boost/asio.hpp>
#include <slint.h>
#include <slint_platform.h>

static const VideoRenderer::Config camwin_opts = {.name = "camera video",
                                                  .width = 600,
                                                  .height = 400,
                                                  .use_opengl = false,
                                                  .dump = false,
                                                  .hide = true};
static const VideoRenderer::Config capwin_opts = {.name = "remote desktop",
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
static const CameraCapturer::Config camera_opts = {
    .width = 600, .height = 400, .fps = 30, .uniq = ""};
ABSL_FLAG(std::string, user, "Morisa", "signaling name");
ABSL_FLAG(std::string, host, "10.10.10.133", "signal server host");
ABSL_FLAG(int, port, 8888, "signal server port");
ABSL_FLAG(bool, auto_login, true, "auto login");
ABSL_FLAG(bool, use_opengl, true, "use OpenGL instead of SDL2");
ABSL_FLAG(bool, use_h264, false, "use custom H264 codec implementation");
ABSL_FLAG(std::vector<std::string>, servers,
          std::vector<std::string>({
              "stun:stun1.l.google.com:19302",
              "stun:stun2.l.google.com:19302",
              "stun:stun3.l.google.com:19302",
          }),
          "stun servers");

MainWindow::MainWindow(int argc, char *argv[]) : app_(App::create())
{
    need_login_ = absl::GetFlag(FLAGS_auto_login);
    chatbuf_.reserve(6400);

    pc_conf_.stun_servers = absl::GetFlag(FLAGS_servers);
    pc_conf_.use_codec = absl::GetFlag(FLAGS_use_h264);
    cc_conf_.host = absl::GetFlag(FLAGS_host);
    cc_conf_.port = absl::GetFlag(FLAGS_port);
    cc_conf_.name = absl::GetFlag(FLAGS_user);

    app_ = App::create();
    app_->set_user(slint::SharedString(cc_conf_.name));
    app_->set_host(slint::SharedString(absl::GetFlag(FLAGS_host)));
    app_->set_port(slint::SharedString::from_number(absl::GetFlag(FLAGS_port)));
    global().on_login([this] { login(); });
    global().on_logout([this] { logout(); });
    global().on_exit([this] { stop(); });
    global().on_connect(
        [this](const slint::SharedString &id) { connect(id.data()); });
    global().on_open_chat([this] { open_chat(); });
    global().on_open_stat([this] { open_stat(); });

    app_->show();

    // TODO: delay heavy works
    pc_ = std::make_unique<PeerClient>(pc_conf_);
    cc_ = std::make_unique<SignalClient>(ioctx_, cc_conf_);
    screen_renderer_ = VideoRenderer::Create(capwin_opts);
    screen_video_src_ = ScreenCapturer::Create(capture_opts);

    ee_ = EventExecutor::create(capwin_opts.width, capwin_opts.height,
                                capture_opts.width, capture_opts.height);
    stats_observer_ = StatsObserver::Create(stats_json_);

    cc_->set_ui_observer_(this);
    cc_->set_peer_observer(pc_.get()); // recursive reference?
    pc_->set_signaling_observer(cc_.get());
    pc_->set_stats_observer(stats_observer_.get());

    pc_->add_screen_video_source(screen_video_src_);
    pc_->add_screen_sinks(screen_renderer_);
    auto cameras = CameraCapturer::GetDeviceList();
    if (!cameras.empty()) {
        logger::info("supported cameras: {}", cameras);
        auto opts = camera_opts;
        opts.uniq = cameras[0].second.c_str();
        camera_renderer_ = VideoRenderer::Create(camwin_opts);
        camera_video_src_ = CameraCapturer::Create(opts);
        pc_->add_camera_video_source(camera_video_src_);
        pc_->add_camera_sinks(camera_renderer_);
    }
}

void MainWindow::login()
{
    need_login_ = false;
    auto port = std::atoi(app_->get_port().data());
    auto host = app_->get_host().data();

    cc_->set_name(app_->get_user().data());
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

void MainWindow::open_stat()
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
    auto update_stats = [this]() {
        if (cc_->calling()) {
            pc_->get_stats();
        }
    };

    auto toggle_grab = [this] {
        auto window = screen_renderer_->get_window();
        auto state = SDL_GetWindowGrab(window);
        SDL_SetWindowGrab(window, state ? SDL_FALSE : SDL_TRUE);
        logger::debug("escape shortcuts met, {} grab mode",
                      state ? "leaving" : "entering");
    };

    auto poll = [=, this]() {
        SDL_Event e;
        EventExecutor::Event ee;
        std::optional<PeerClient::ChanMessage> msg;

        while (ioctx_.poll()) {
            ;
        }

        while (pc_ && (msg = pc_->poll_remote_message())) {
            if (msg->binary) {
                ee.native_ev = *reinterpret_cast<SDL_Event *>(
                    const_cast<uint8_t *>(msg.value().data));

                ee_->execute(ee);
            } else {
                std::string text((const char *)msg->data, msg->size);
                update_chat(cc_->peer().name, text.c_str());
            }
        }

        while (SDL_PollEvent(&e)) {
            handle_remote_event(e);
        }

        app_->set_signed(cc_->online());

        screen_renderer_->update_frame();
    };

    Trigger::on({SDLK_LCTRL, SDLK_LSHIFT, SDLK_LALT, SDLK_q}, toggle_grab);
    slint::Timer stats_timer(std::chrono::seconds(5), update_stats);
    slint::Timer poll_timer(std::chrono::milliseconds(0), poll);
    auto work = boost::asio::make_work_guard(ioctx_);

    slint::run_event_loop();
}

void MainWindow::handle_remote_event(SDL_Event &e)
{
    Trigger::processEvent(e);
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
}

static PeerData to_peer_data(const Peer &p)
{
    return {
        slint::SharedString(p.name),
        slint::SharedString(p.id),
        p.online,

    };
}

void MainWindow::OnPeersChanged(Peer::List peers)
{
    slint::invoke_from_event_loop([this, peers = std::move(peers)] {
        auto model = std::make_shared<slint::VectorModel<PeerData>>();
        for (const auto &[id, p] : peers) {
            model->push_back(to_peer_data(p));
        }
        app_->set_peers(model);
    });
}

void MainWindow::OnLogin(Peer me)
{
    slint::invoke_from_event_loop([this, me] { app_->set_signed(me.online); });
}

void MainWindow::OnLogout(Peer me)
{
    slint::invoke_from_event_loop([this, me] { app_->set_signed(me.online); });
}
