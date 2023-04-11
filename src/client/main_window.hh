#pragma once

#include <memory>
#include <string>
#include <thread>

#include "peer_client.hh"
#include "signal_client.hh"
#include "video_renderer.hh"

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

  private:
    std::string title_;
    mu_Context *ctx_;
    boost::asio::io_context io_ctx_;
    std::unique_ptr<std::thread> thread_;
    std::unique_ptr<SignalClient> cc_;
    rtc::scoped_refptr<PeerClient> pc_;
    std::unique_ptr<VideoRenderer> local_renderer_;
    std::unique_ptr<VideoRenderer> remote_renderer_;
};
