#pragma once

#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#elif _WIN32

#endif

#include "common.h"

namespace blitz
{
#ifdef __linux__
    
    struct LinuxAcceptorImpl
    {
        int sockfd;
        sockaddr_in addr;

        LinuxAcceptorImpl();
        void bind(std::uint16_t port);
        void listen(int backlog);
    };
    
    using AcceptorImpl = LinuxAcceptorImpl;
        
#elif _WIN32


        
#endif

    class Acceptor : public Event
    {
    public:
        Acceptor(); // 设置事件为ACCEPT
        Acceptor(const Acceptor&) = delete;
        Acceptor& operator=(const Acceptor&) = delete;
        Acceptor(Acceptor&& rhs);
        Acceptor& operator=(Acceptor&& rhs);
        ~Acceptor();

        void listen(std::uint16_t port, int backlog = 5);

    private:
        AcceptorImpl impl_;
    };
}   // namespace blitz
