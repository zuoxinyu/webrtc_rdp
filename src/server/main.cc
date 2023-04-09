#include "chat_server.hh"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    boost::asio::io_context ioc;

    std::string host = "0.0.0.0";
    int port = 8888;

    if (argc > 2) {
        host = argv[1];
        port = atoi(argv[2]);
    }

    ChatServer server(ioc, host, port);
    boost::asio::co_spawn(ioc, server.do_listen(), boost::asio::detached);

    ioc.run();
}
