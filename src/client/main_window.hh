#pragma once

#include "client/stats/stats.hh"
#include "event_executor.hh"
#include "peer_client.hh"
#include "signal_client.hh"
#include "sink/video_renderer.hh"
#include "source/camera_capturer.hh"
#include "source/screen_capturer.hh"

#include <memory>
#include <string>
#include <thread>

using mu_Context = struct mu_Context;

class MainWindow
{
  public:
    MainWindow(mu_Context *ctx_, int argc, char *argv[]);
    ~MainWindow() = default;

    void run();
    void stop();

  private:
    // windows
    void peers_window(mu_Context *ctx);
    void login_window(mu_Context *ctx);
    void chat_window(mu_Context *ctx);
    void stats_window(mu_Context *ctx);

    // handlers
    void process_mu_windows();
    void process_mu_commands();
    void handle_main_event(const SDL_Event &e);
    void handle_remote_event(const SDL_Event &e);

    // actions
    void login();
    void logout();
    void connect(const Peer::Id &);
    void disconnect();
    void update_chat(const std::string &who, const char *buf);
    void open_stats();
    void open_chat();

  private:
    std::string title_;
    bool auto_login_ = false;
    mu_Context *ctx_ = nullptr;
    PeerClient::Config pc_conf_;

    boost::asio::io_context ioctx_;
    std::unique_ptr<SignalClient> cc_ = nullptr;
    std::unique_ptr<PeerClient> pc_ = nullptr;
    std::unique_ptr<EventExecutor> executor_ = nullptr;
    rtc::scoped_refptr<CameraCapturer> camera_video_src_ = nullptr;
    rtc::scoped_refptr<ScreenCapturer> screen_video_src_ = nullptr;
    rtc::scoped_refptr<VideoRenderer> camera_renderer_ = nullptr;
    rtc::scoped_refptr<VideoRenderer> screen_renderer_ = nullptr;
    rtc::scoped_refptr<StatsObserver> stats_observer_ = nullptr;

    // states
    bool running_ = false;
    std::vector<char> chatbuf_;
    bool chatbuf_updated_ = false;
    bool show_stats_ = false;
    std::string stats_json_;
};
