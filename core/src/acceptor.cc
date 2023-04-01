#include "acceptor.h"
#include <cstring>
#include <utility>
#include "ec.h"
#include <iostream>

namespace blitz
{
#ifdef __linux__

    LinuxAcceptorImpl::LinuxAcceptorImpl()
        : sockfd{::socket(AF_INET, SOCK_STREAM, 0)}
    {
        if (-1 == sockfd)
        {
            throw std::system_error(make_error_code(SocketError::InternalError));
        }
    }

    void LinuxAcceptorImpl::bind(std::uint16_t port)
    {
        ::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(port);
        addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
        if (-1 == ::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)))
        {
            throw std::system_error(make_error_code(SocketError::InternalError));
        }
    }

    void LinuxAcceptorImpl::listen(int backlog)
    {
        if (-1 == ::listen(sockfd, backlog))
        {
            throw std::system_error(make_error_code(SocketError::InternalError));
        }
    }
        
#elif _WIN32


        
#endif

    Acceptor::Acceptor()
        : Event{-1}, impl_{}
    {
        this->mSocket_ = this->impl_.sockfd;
        this->setEvent(EventType::ACCEPT);
    }

    Acceptor::Acceptor(Acceptor&& rhs)
        : Event{rhs.mSocket_}
    {
        *this = std::move(rhs);
    }

    Acceptor& Acceptor::operator=(Acceptor&& rhs)
    {
        if (this != &rhs)
        {
            this->mSocket_ = rhs.mSocket_;
            this->mCurEvent_ = rhs.mCurEvent_;
            this->impl_ = std::move(rhs.impl_);
        }
        return *this;
    }

    Acceptor::~Acceptor()
    {
        this->setEvent(EventType::EMPTY);
    }

    void Acceptor::listen(std::uint16_t port, int backlog)
    {
        this->impl_.bind(port);
        this->impl_.listen(backlog);
    }
}   // namespace blitz
