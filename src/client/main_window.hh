#include <memory>
#include <string>
#include <thread>

#include "chat_client.hh"
#include "peer_client.hh"
#include "video_renderer.hh"

typedef struct mu_Context mu_Context;

class MainWindow
{
  public:
    MainWindow(mu_Context *ctx_, const std::string &title = "myname");
    ~MainWindow() {}

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
    std::unique_ptr<ChatClient> cc_;
    std::unique_ptr<VideoRenderer> local_renderer_;
    std::unique_ptr<VideoRenderer> remote_renderer_;
    rtc::scoped_refptr<PeerClient> pc_;
};
