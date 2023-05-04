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

namespace blitz
{
#ifdef __linux__
    constexpr static int QUEUE_SIZE = 64;

    int SignalEvent::curSig;
    int SignalEvent::sigFd[2];

    SignalEvent::SignalEvent() 
        : Event{0} 
    { 
        this->setEvent(EventType::SIGNAL); 
        if (0 != ::pipe(sigFd))
        {
            throw std::system_error(make_error_code(ErrorCode::InternalError));
        }
        int old_option = ::fcntl(sigFd[1], F_GETFL);
        ::fcntl(sigFd[1], F_SETFL, old_option | O_NONBLOCK);
    }

    SignalEvent::~SignalEvent()
    {
        ::close(sigFd[0]);
        ::close(sigFd[1]);
    }

    SignalEvent& SignalEvent::instance() noexcept 
    { 
        static SignalEvent ev; 
        return ev; 
    }

    void SignalEvent::SignalHandle(int sig) noexcept 
    {
        ::write(SignalEvent::sigFd[1], &sig, sizeof(sig));
    }

    int& SignalEvent::curSignal() noexcept { return curSig; }
    int SignalEvent::readPipe() noexcept { return sigFd[0]; }

    int TickEvent::timerFd;
    std::uint64_t TickEvent::timeoutCnt;
    struct itimerspec TickEvent::ts;

    TickEvent::TickEvent()
        : Event{0}
    {
        if (timerFd = ::timerfd_create(CLOCK_MONOTONIC, 0); -1 == timerFd)
        {
            throw std::system_error(make_error_code(ErrorCode::InternalError));
        }
        this->setEvent(EventType::TIMEOUT);
    }

    TickEvent::~TickEvent()
    {
        ::close(timerFd);
    }

    TickEvent& TickEvent::instance() noexcept
    {
        static TickEvent ev; 
        return ev;
    }

    int TickEvent::fd() { return timerFd; }
    std::uint64_t& TickEvent::tickCount() { return timeoutCnt; }

    void TickEvent::setTimer(int tickTimeMs)
	{
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        ts.it_value.tv_sec = tickTimeMs / 1000;
        ts.it_value.tv_nsec = (tickTimeMs % 1000) * 1000000;
        if (-1 == ::timerfd_settime(timerFd, 0, &ts, nullptr)) 
        {
            ::close(timerFd);
        }
	}

	void TickEvent::stopTimer()
	{
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = 0;
        if (-1 == ::timerfd_settime(timerFd, 0, &ts, nullptr)) 
        {
            ::close(timerFd);
        }
	}

    LinuxEventQueue::LinuxEventQueue()
        : mCompletionQueue_{nullptr}
    {
        if (int err = ::io_uring_queue_init(QUEUE_SIZE, &this->mRing_, 0); 0 != err)
        {
            errno = -err;
            throw std::system_error(make_error_code(ErrorCode::InternalError));
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
                else if (event->isClosed() || event->isSignal() || event->isTick())
                {
                    ret = event;
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
            return ErrorCode::InternalError;
        }
        else
        {
            return ErrorCode::Success;
        }
    }

    std::error_code LinuxEventQueue::submitAccept(Acceptor& acceptor)
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe) 
        {
            return ErrorCode::SubmitQueueFull;
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
            return ErrorCode::SubmitQueueFull;
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
            return ErrorCode::SubmitQueueFull;
        }
        ::io_uring_prep_close(sqe, conn->socket());
        return SubmitHelper(&this->mRing_, sqe, conn);
    }

    std::error_code LinuxEventQueue::submitSysSignal(int sig)
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)
        {
            return ErrorCode::SubmitQueueFull;
        }
        ::signal(sig, &SignalEvent::SignalHandle);
        auto& sev = SignalEvent::instance();
        ::io_uring_prep_read(sqe, sev.readPipe(), &sev.curSignal(), sizeof(sev.curSignal()), 0);
        auto ec = SubmitHelper(&this->mRing_, sqe, &sev);
        if (ec != ErrorCode::Success)
        {
            ::signal(sig, SIG_DFL);
        }
        return ec;
    }

    std::error_code LinuxEventQueue::submitTimerTick()
    {
        auto* sqe = ::io_uring_get_sqe(&this->mRing_);
        if (!sqe)
        {
            return ErrorCode::SubmitQueueFull;
        }
        auto& tev = TickEvent::instance();
        ::io_uring_prep_read(sqe, tev.fd(), &tev.tickCount(), sizeof(tev.tickCount()), 0);
        return SubmitHelper(&this->mRing_, sqe, &tev);
    }

#elif _WIN32



#endif
}   // namespace blitz
