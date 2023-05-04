#include <string>
#include <mutex>
#include <iostream>
#include "server.h"
#include "connection.h"

int main()
{
    using namespace std::chrono_literals;
    std::string data = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nblitz";
    std::uint16_t port = 8888;
    std::size_t threadNum = 7;
    std::mutex mt;

    blitz::TcpServer svr{threadNum, port};
    svr.setSignalCallback(SIGPIPE, [&mt]()->void 
    { 
        std::lock_guard l{mt};
        std::cout << "Caught signal SIGPIPE" << std::endl; 
    });
    svr.setSignalCallback(SIGINT, [&svr]()->void 
    { 
        std::cout << "Caught signal SIGINT" << std::endl; 
        svr.stop(); 
    });

    svr.setReadCallback([&data, &mt](blitz::Connection* conn)->void
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

    svr.setWriteCallback([&mt](blitz::Connection* conn)->void
    {
        std::lock_guard l{mt};
        std::cout << "write done" << std::endl;
        conn->close();
    });

    svr.setErrorCallback([&mt](blitz::Connection* conn, std::error_code ec)->void
    {
        std::lock_guard l{mt};
        std::cout << ec.message() << std::endl;
    });

    svr.setTimeoutCallback([&mt](blitz::Connection* conn)->void
    {
        std::lock_guard l{mt};
        std::cout << "connection time out" << std::endl;
    }, 1000ms);

    svr.run(100ms);
    return 0;
}
