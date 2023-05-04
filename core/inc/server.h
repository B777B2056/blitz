#pragma once
#include <cstddef>
#include <memory>
#include "acceptor.h"
#include "threadpool.h"
#include "timer.h"

#ifdef __linux__
    #define SIGNAL_NUM 32
#elif _WIN32
    #define SIGNAL_NUM 6
#endif

namespace blitz
{
    class TcpServer
    {
    public:
        TcpServer(std::size_t threadNum, std::uint16_t port, int backlog = 5);

        void run(std::chrono::milliseconds tickMs);
        void stop();

        void setReadCallback(IoEventCallback cb) noexcept;
        void setWriteCallback(IoEventCallback cb) noexcept;
        void setErrorCallback(ErrorCallback cb) noexcept;
        void setSignalCallback(int sig, SignalCallback cb) noexcept;
        void setTimeoutCallback(TimeoutCallback cb, std::chrono::milliseconds timeoutMs) noexcept;
    
    private:
        EventQueue mMainEventQueue_;
        Acceptor mAcceptor_;
        Timer mTimer_;
        std::unique_ptr<IoServicePool> mPool_;
        SignalCallback mSignalCbs_[SIGNAL_NUM];
        bool isStopLoop_;

        void startTimer(std::chrono::milliseconds tickMs);
    };
}   // namespace blitz
#undef SIGNAL_NUM
