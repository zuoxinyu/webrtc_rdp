#include "chat_server.hh"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    boost::asio::io_context ioc;

    ChatServer server(ioc, "127.0.0.1", 8888);
    boost::asio::co_spawn(ioc, server.do_listen(), boost::asio::detached);

    ioc.run();
}
