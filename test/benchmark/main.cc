#include <string>
#include <csignal>
#include <cstdio>
#include "io_service.h"
#include "connection.h"

std::string data = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nblitz";

void signal_callback_handler(int signum)
{
    std::printf("Caught signal SIGPIPE %d\n",signum);
}

int main()
{
    std::uint16_t port = 8888;
    // unsigned int threadNum = 4;
    ::signal(SIGPIPE, signal_callback_handler);
    blitz::Acceptor acceptor;
    acceptor.listen(port);
    auto svr = blitz::IoService{acceptor};
    svr.registReadCallback([](blitz::Connection* conn)->void
    {
        char ch;
        std::error_code ec;
        int state = 0;
        while (state != 4)
        {
            conn->read(std::span{&ch, 1}, ec);
            if (ec == blitz::SocketError::PeerClosed)
            {
                return;
            }
            if ((ch == '\r') || (ch == '\n'))
            {
                ++state;
            } 
            else
            {
                state = 0;
            }
        }
        conn->write(std::span{data.data(), data.size()}, ec);
    });
    svr.registWriteCallback([&svr](blitz::Connection* conn)->void
    {
        svr.closeConn(conn);
    });
    svr.run();
    return 0;
}
