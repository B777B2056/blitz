#pragma once
#include <cstdint>
#include <functional>
#include <system_error>

namespace blitz
{
#ifdef __linux__
    using SocketDescriptor = int;
#elif _WIN32
    using SocketDescriptor = int;
#endif

    enum class SocketOption : std::uint8_t
    {
        BLOCKING = 0, 
        NONBLOCKING,
        TCP_NO_DELAY,
        KEEPALIVE,
        REUSE_ADDR,
        REUSE_PORT,
    };

    enum class EventType : std::uint8_t 
    {
        EMPTY = 0,
        ACCEPT,
        READ,
        WRITE,
        CLOSING,
        CLOSED,
        TIMEOUT,
        SIGNAL
    };

    class Connection;

    using SignalCallback = std::function<void()>;
    using IoEventCallback = std::function<void(Connection* conn)>;
    using ErrorCallback = std::function<void(Connection* conn, std::error_code ec)>;

    class Event
    {
    protected:
        EventType mCurEvent_;
        SocketDescriptor mSocket_;

    public:
        Event(SocketDescriptor socket) : mSocket_{socket} {}
        virtual ~Event() {}

        EventType event() const { return this->mCurEvent_; }
        void setEvent(EventType ev) { this->mCurEvent_ = ev; }

        void setSocket(SocketDescriptor sock) { this->mSocket_ = sock; }
        SocketDescriptor socket() const { return this->mSocket_; }

        bool isNull() const { return this->mCurEvent_ == EventType::EMPTY; }
        bool isAccept() const { return this->mCurEvent_ == EventType::ACCEPT; }
        bool isRead() const { return this->mCurEvent_ == EventType::READ; }
        bool isWrite() const { return this->mCurEvent_ == EventType::WRITE; }
        bool isClosing() const { return this->mCurEvent_ == EventType::CLOSING; }
        bool isClosed() const { return this->mCurEvent_ == EventType::CLOSED; }
        bool isTick() const { return this->mCurEvent_ == EventType::TIMEOUT; }
        bool isSignal() const { return this->mCurEvent_ == EventType::SIGNAL; }
    };
}
