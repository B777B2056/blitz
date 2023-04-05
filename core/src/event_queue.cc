#include "event_queue.h"
#include <cerrno>
#include <cstring>
#include <iostream>

#ifdef __linux__
#include <unistd.h>
#elif _WIN32

#endif

#include "acceptor.h"
#include "connection.h"
#include "sig_handler.h"

namespace blitz
{
#ifdef __linux__
    static int SIGNAL_FD[2];
    static int CURRENT_SIGNAL;
    static SignalHandler SignalEvent{0};
    constexpr static int QUEUE_SIZE = 512;

    static void SignalHandle(int sig) noexcept 
    {
        ::write(SIGNAL_FD[1], &sig, sizeof(sig));
    }

    LinuxEventQueue::LinuxEventQueue()
        : mCompletionQueue_{nullptr}
    {
        if (int err = ::io_uring_queue_init(QUEUE_SIZE, &this->mRing_, 0); 0 != err)
        {
            errno = -err;
            throw std::system_error(make_error_code(ErrorCode::InternalError));
        }
        if (0 != ::pipe(SIGNAL_FD))
        {
            throw std::system_error(make_error_code(ErrorCode::InternalError));
        }
        int old_option = ::fcntl(SIGNAL_FD[1], F_GETFL);
        ::fcntl(SIGNAL_FD[1], F_SETFL, old_option | O_NONBLOCK);
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

    Event* LinuxEventQueue::waitCompletionEvent(std::error_code& ec)
    {
        Event* ret = nullptr;
        ec = ErrorCode::Success;
        if (int err = ::io_uring_wait_cqe(&this->mRing_, &this->mCompletionQueue_); err < 0) 
        {
            errno = -err;
            ec = ErrorCode::InternalError;
        } 
        else
        {
            auto* event = reinterpret_cast<Event*>(::io_uring_cqe_get_data(this->mCompletionQueue_));
            if (!event) goto END;
            if (this->mCompletionQueue_->res < 0)
            {
                if (this->mCompletionQueue_->res == -ECONNRESET || this->mCompletionQueue_->res == -ENOTCONN)
                {
                    ec = ErrorCode::PeerClosed;
                }
                else
                {
                    errno = -this->mCompletionQueue_->res;
                    ec = ErrorCode::InternalError;
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
                    ret = event;
                }
                else if (event->isSignal())
                {
                    auto sigEv = static_cast<SignalHandler*>(event);
                    sigEv->setSignal(CURRENT_SIGNAL);
                    ret = sigEv;
                }
                else
                {
                    ret = this->handleIo(event);
                }
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

    static std::error_code SubmitHelper(struct io_uring* ring, struct io_uring_sqe* sqe, void* data)
    {
        ::io_uring_sqe_set_data(sqe, data);
        if (int ret = ::io_uring_submit(ring); ret < 0)
        {
            errno = -ret;
            return make_error_code(ErrorCode::InternalError);
        }
        else
        {
            return make_error_code(ErrorCode::Success);
        }
    }

    std::error_code LinuxEventQueue::submitAccept(Acceptor& acceptor)
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe) 
        {
            return make_error_code(ErrorCode::SubmitQueueFull);
        }
        ::io_uring_prep_accept(sqe, acceptor.socket(), nullptr, nullptr, 0);
        return SubmitHelper(&this->mRing_, sqe, &acceptor);
    }

    // 内核向用户读缓冲区写入数据
    static void ReadFromKernel(struct io_uring_sqe* sqe, Connection* conn)
    {
        auto iovecs = conn->readBuffer().writeableArea2Iovecs();   
        ::io_uring_prep_readv(sqe, conn->socket(), iovecs.data(), iovecs.size(), 0);
    }

    // 内核从用户写缓冲区读出数据
    static void WriteIntoKernel(struct io_uring_sqe* sqe, Connection* conn)
    {
        auto iovecs = conn->writeBuffer().readableArea2Iovecs();   
        ::io_uring_prep_writev(sqe, conn->socket(), iovecs.data(), iovecs.size(), 0);
    }

    std::error_code LinuxEventQueue::submitIoEvent(Connection* conn)
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)
        {
            return make_error_code(ErrorCode::SubmitQueueFull);
        }
        if (conn->isRead())
        {
            ReadFromKernel(sqe, conn);
        }
        else if (conn->isWrite())
        {
            WriteIntoKernel(sqe, conn);
        }
        return SubmitHelper(&this->mRing_, sqe, conn);
    }

    std::error_code LinuxEventQueue::submitCloseConn(Connection* conn)
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)
        {
            return make_error_code(ErrorCode::SubmitQueueFull);
        }
        ::io_uring_prep_close(sqe, conn->socket());
        return SubmitHelper(&this->mRing_, sqe, conn);
    }

    std::error_code LinuxEventQueue::submitSysSignal(int sig)
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)
        {
            return make_error_code(ErrorCode::SubmitQueueFull);
        }
        ::signal(sig, SignalHandle);
        ::io_uring_prep_read(sqe, SIGNAL_FD[0], &CURRENT_SIGNAL, sizeof(CURRENT_SIGNAL), 0);
        SignalEvent.setEvent(EventType::SIGNAL);
        SignalEvent.setSocket(SIGNAL_FD[0]);
        auto ec = SubmitHelper(&this->mRing_, sqe, &SignalEvent);
        if (ec != ErrorCode::Success)
        {
            ::signal(sig, SIG_DFL);
        }
        return ec;
    }

#elif _WIN32



#endif
}   // namespace blitz
