#include "event_queue.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include "acceptor.h"
#include "connection.h"

namespace blitz
{
#ifdef __linux__

    constexpr static int QUEUE_SIZE = 512;

    LinuxEventQueue::LinuxEventQueue()
        : acceptorDescriptor_{-1}, mCompletionQueue_{nullptr}
    {
        if (int err = ::io_uring_queue_init(QUEUE_SIZE, &this->mRing_, 0); 0 != err)
        {
            errno = -err;
            throw std::system_error(make_error_code(SocketError::InternalError));
        }
    }

    LinuxEventQueue::LinuxEventQueue(LinuxEventQueue&& rhs)
        : mCompletionQueue_{nullptr}
    {
        *this = std::move(rhs);
    }

    LinuxEventQueue& LinuxEventQueue::operator=(LinuxEventQueue&& rhs)
    {
        if (this != &rhs)
        {
            this->mRing_ = std::move(rhs.mRing_);
            this->mCompletionQueue_ = rhs.mCompletionQueue_;
            rhs.mCompletionQueue_ = nullptr;
        }
        return *this;
    }

    LinuxEventQueue::~LinuxEventQueue()
    {
        ::io_uring_queue_exit(&this->mRing_);
    }

    Event* LinuxEventQueue::waitCompletionEvent()
    {
        if (int err = ::io_uring_wait_cqe(&this->mRing_, &this->mCompletionQueue_); 0 != err) 
        {
            errno = -err;
            throw std::system_error(make_error_code(SocketError::InternalError));
        }
        auto* event = reinterpret_cast<Event*>(::io_uring_cqe_get_data(this->mCompletionQueue_));
        Event* ret = nullptr;
        if (!event) goto END;
        if (this->mCompletionQueue_->res < 0)
        {
            if (this->mCompletionQueue_->res == -ECONNRESET || this->mCompletionQueue_->res == -ENOTCONN)
            {
                this->closeConn(static_cast<Connection*>(event));
            }
            else
            {
                std::cerr << ::strerror(-this->mCompletionQueue_->res) << std::endl;
            }
        }
        else
        {
            if (event->isAccept())
            {
                ret = this->handleAccept(event);
            } 
            else if (event->isClosed())
            {
                ret = this->handleClosed(event);
            }
            else
            {
                ret = this->handleIo(event);
            }
        }
    END:
        ::io_uring_cqe_seen(&this->mRing_, this->mCompletionQueue_);
        return ret;
    }

    Event* LinuxEventQueue::handleAccept(Event* event)
    {
        // 连接完成事件
        auto* clt = new Connection(this->mCompletionQueue_->res);
        clt->setEvent(EventType::ACCEPT);
        return clt;
    }

    Event* LinuxEventQueue::handleClosed(Event* event)
    {
        // 连接关闭完成事件
        return event;
    }

    Event* LinuxEventQueue::handleIo(Event* event)
    {
        // IO完成事件
        std::size_t transferredBytes = this->mCompletionQueue_->res;
        if (event->isRead())
        {   
            // 内核向用户读缓冲区写入数据
            static_cast<Connection*>(event)->readBuffer().moveWriteableAreaIdx(transferredBytes);
            static_cast<Connection*>(event)->readBuffer().destroyWriteableIovecs();
        }
        else if (event->isWrite())
        {
            // 内核从用户写缓冲区读出数据
            static_cast<Connection*>(event)->writeBuffer().moveReadableAreaIdx(transferredBytes);
            static_cast<Connection*>(event)->writeBuffer().destroyReadableIovecs();
        }
        return event;
    }

    int LinuxEventQueue::submitAccept(Acceptor& acceptor)
    {
        socklen_t len;
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)   return -1;
        ::io_uring_prep_accept(sqe, acceptor.socket(), 
                                reinterpret_cast<struct sockaddr*>(&acceptor.addr()), &len, 0);
        ::io_uring_sqe_set_data(sqe, &acceptor);
        this->acceptorDescriptor_ = acceptor.socket();
        return ::io_uring_submit(&this->mRing_);
    }

    int LinuxEventQueue::submitIoEvent(Connection* conn)
    {
        if (!conn)  return -1;
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)   return -1;
        if (conn->isRead())
        {
            this->readFromKernel(sqe, conn);
        }
        else if (conn->isWrite())
        {
            this->writeIntoKernel(sqe, conn);
        }
        ::io_uring_sqe_set_data(sqe, conn);
        return ::io_uring_submit(&this->mRing_);
    }

    void LinuxEventQueue::readFromKernel(struct io_uring_sqe* sqe, Connection* conn)
    {
        auto iovecs = conn->readBuffer().writeableArea2Iovecs();   // 内核向用户读缓冲区写入数据
        ::io_uring_prep_readv(sqe, conn->socket(), iovecs.data(), iovecs.size(), 0);
    }

    void LinuxEventQueue::writeIntoKernel(struct io_uring_sqe* sqe, Connection* conn)
    {
        auto iovecs = conn->writeBuffer().readableArea2Iovecs();   // 内核从用户写缓冲区读出数据
        ::io_uring_prep_writev(sqe, conn->socket(), iovecs.data(), iovecs.size(), 0);
    }

    void LinuxEventQueue::closeConn(Connection* conn)
    {
        if (!conn)  return;
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)   return;
        conn->setEvent(EventType::CLOSED);
        ::io_uring_prep_close(sqe, conn->socket());
        ::io_uring_sqe_set_data(sqe, conn);
        ::io_uring_submit(&this->mRing_);
    }

#elif _WIN32



#endif
}   // namespace blitz
