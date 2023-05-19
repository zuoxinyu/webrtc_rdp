#pragma once

#include "executor/event_executor.hh"
#include "peer_client.hh"
#include "signal_client.hh"
#include "sink/video_renderer.hh"
#include "source/camera_capturer.hh"
#include "source/screen_capturer.hh"
#include "stats/stats.hh"

#include "ui/app.slint.h"

#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using mu_Context = struct mu_Context;

class MainWindow : public UIObserver
{
  public:
    MainWindow(int argc, char *argv[]);
    ~MainWindow() = default;

    void run();
    void stop();

  public:
    void OnLogout(Peer me) override;
    void OnLogin(Peer me) override;
    void OnPeersChanged(Peer::List peers) override;

  private:
    // handlers
    void handle_remote_event(SDL_Event &e);

    // actions
    void login();
    void logout();
    void connect(const Peer::Id &);
    void disconnect();
    void update_chat(const std::string &who, const char *buf);
    void post_chat(const std::string &msg);

    // misc
    const ClientState &global() {return app_->global<ClientState>(); };

  private:
    // properties
    std::string title_;
    PeerClient::Config pc_conf_;
    SignalClient::Config cc_conf_;

    // resources
    boost::asio::io_context ioctx_;
    std::unique_ptr<SignalClient> cc_ = nullptr;
    std::unique_ptr<PeerClient> pc_ = nullptr;
    std::unique_ptr<EventExecutor> ee_ = nullptr;
    rtc::scoped_refptr<CameraCapturer> camera_video_src_ = nullptr;
    rtc::scoped_refptr<ScreenCapturer> screen_video_src_ = nullptr;
    rtc::scoped_refptr<VideoRenderer> camera_renderer_ = nullptr;
    rtc::scoped_refptr<VideoRenderer> screen_renderer_ = nullptr;
    rtc::scoped_refptr<StatsObserver> stats_observer_ = nullptr;

    // slint ui
    slint::ComponentHandle<App> app_;

    // states
    bool need_login_ = false;
    bool running_ = false;
    std::vector<char> chatbuf_;
    bool chatbuf_updated_ = false;
    bool show_stats_ = false;
    std::string stats_json_;
};
