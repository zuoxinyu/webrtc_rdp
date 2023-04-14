#pragma once

#include "capturer/camera_capturer.hh"
#include "capturer/video_capturer.hh"
#include "peer_client.hh"
#include "signal_client.hh"
#include "sink/video_renderer.hh"

#include <memory>
#include <string>
#include <thread>

#include <SDL2/SDL_events.h>

using mu_Context = struct mu_Context;

class MainWindow
{
  public:
    MainWindow(mu_Context *ctx_, const std::string &title = "myname");
    ~MainWindow() = default;

    void run();

  private:
    void render_windows(mu_Context *ctx);
    void peers_window(mu_Context *ctx);
    void login_window(mu_Context *ctx);
    void handle_mu_events();
    void handle_main_event(const SDL_Event &e);
    void handle_remote_event(const SDL_Event &e);

  private:
    std::string title_;
    mu_Context *ctx_;
    boost::asio::io_context io_ctx_;
    std::unique_ptr<std::thread> thread_;
    std::unique_ptr<SignalClient> cc_;
    rtc::scoped_refptr<PeerClient> pc_;
    rtc::scoped_refptr<CameraCapturer> local_video_src_ = nullptr;
    rtc::scoped_refptr<ScreenCapturer> remote_video_src_ = nullptr;
    std::unique_ptr<VideoRenderer> local_renderer_ = nullptr;
    std::unique_ptr<VideoRenderer> remote_renderer_ = nullptr;
};
