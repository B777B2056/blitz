#pragma once

#ifdef __linux__
#include <liburing.h>
#elif _WIN32

#endif
#include "common.h"
#include "ec.h"

namespace blitz
{
    class Acceptor;
    class Connection;
    class ChainBuffer;
    class Event;

#ifdef __linux__
    
    class LinuxEventQueue
    {
    public:
        LinuxEventQueue();
        LinuxEventQueue(const LinuxEventQueue&) = delete;
        LinuxEventQueue& operator=(const LinuxEventQueue&) = delete;
        LinuxEventQueue(LinuxEventQueue&& rhs);
        LinuxEventQueue& operator=(LinuxEventQueue&& rhs);
        ~LinuxEventQueue();

        Event* waitCompletionEvent(std::error_code& ec);
        std::error_code submitAccept(Acceptor& acceptor);
        std::error_code submitIoEvent(Connection* conn);
        std::error_code submitCloseConn(Connection* conn);

    private:
        struct io_uring mRing_;
        struct io_uring_cqe* mCompletionQueue_;

        Event* handleAccept(Event* event);
        Event* handleClosed(Event* event);
        Event* handleIo(Event* event);

        struct iovec* chainBuffer2ReadIovecs(ChainBuffer& buf, std::size_t& len);
        struct iovec* chainBuffer2WriteIovecs(ChainBuffer& buf, std::size_t& len);
    };

    using EventQueueImpl = LinuxEventQueue;

#elif _WIN32

    class WinEventQueue
    {
    public:
        WinEventQueue();
        WinEventQueue(const WinEventQueue&) = delete;
        WinEventQueue& operator=(const WinEventQueue&) = delete;
        WinEventQueue(WinEventQueue&& rhs);
        WinEventQueue& operator=(WinEventQueue&& rhs);
        ~WinEventQueue();

        Event* waitCompletionEvent(std::error_code& ec);
        std::error_code submitAccept(Acceptor& acceptor);
        std::error_code submitIoEvent(Connection* conn);
        std::error_code submitCloseConn(Connection* conn);

    private:
    };

    using EventQueueImpl = WinEventQueue;

#endif

    class EventQueue
    {
    public:
        EventQueue() = default;
        EventQueue(const EventQueue&) = delete;
        EventQueue& operator=(const EventQueue&) = delete;
        EventQueue(EventQueue&&) = default;
        EventQueue& operator=(EventQueue&&) = default;
        ~EventQueue() = default;

        Event* waitCompletionEvent(std::error_code& ec) { return impl_.waitCompletionEvent(ec); }
        std::error_code submitAccept(Acceptor& acceptor) { return impl_.submitAccept(acceptor); }
        std::error_code submitIoEvent(Connection* conn) { return impl_.submitIoEvent(conn); }
        std::error_code submitCloseConn(Connection* conn) { return impl_.submitCloseConn(conn); }
    
    private:
        EventQueueImpl impl_;
    };

}   // namespace blitz
