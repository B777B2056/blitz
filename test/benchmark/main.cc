#include <string>
#include <iostream>
#include "io_service.h"
#include "connection.h"

std::string data = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nblitz";

int main()
{
    std::uint16_t port = 8888;
    unsigned int threadNum = 7;
    blitz::Acceptor acceptor;
    acceptor.listen(port);
    auto svr = blitz::IoService{acceptor, threadNum};
    svr.registSignalCallback(SIGPIPE, []()->void { std::cout << "Caught signal SIGPIPE" << std::endl; });
    svr.registSignalCallback(SIGINT, [&svr]()->void { std::cout << "Caught signal SIGINT" << std::endl; svr.stop(); });
    svr.registReadCallback([](blitz::Connection* conn)->void
    {
        char ch;
        std::error_code ec;
        int state = 0;
        while (state != 4)
        {
            conn->read(std::span{&ch, 1}, ec);
            if (ec == blitz::ErrorCode::PeerClosed)
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
    svr.registErrorCallback([](blitz::Connection* conn, std::error_code ec)->void
    {
        std::cout << ec.message() << std::endl;
    });
    svr.run();
    return 0;
}
