#include "server.h"
#include <iostream>
#include "connection.h"

namespace blitz
{
    TcpServer::TcpServer(std::size_t threadNum, std::uint16_t port, int backlog)
        : mMainEventQueue_{}, mAcceptor_{mMainEventQueue_}
        , mPool_{std::make_unique<IoServicePool>(threadNum)}, isStopLoop_{false}
    {
        this->mAcceptor_.listen(port, backlog);
        this->mAcceptor_.doOnce();
    }

    void TcpServer::run(std::chrono::milliseconds tickMs)
    {
        std::error_code ec;
        this->mPool_->start(this->mTimer_);
        this->startTimer(tickMs);
        while (!this->isStopLoop_)
        {
            Event* ev = this->mMainEventQueue_.waitCompletionEvent(ec);
            if (!ev)    continue;
            if (ec != ErrorCode::Success)
            {
                continue;
            }
            if (ev->isAccept())
            {
                Connection* conn = static_cast<Connection*>(ev);
                this->mPool_->putNewConnection(conn);
                this->mTimer_.add(conn);
                this->mAcceptor_.doOnce();
            }
            else if (ev->isTick())
            {
                this->mTimer_.tick();
                TickEvent::instance().setTimer(tickMs.count());
                this->mMainEventQueue_.submitTimerTick();
            }
            else if (ev->isSignal())
            {
                auto* sigEv = static_cast<SignalEvent*>(ev);
                auto& cb = this->mSignalCbs_[sigEv->curSignal()];
                if (cb) cb();
            }
        }
        std::cout << "run break" << std::endl;
    }

    void TcpServer::stop() 
    { 
        this->isStopLoop_ = true;
        this->mPool_.release();
    }

    void TcpServer::startTimer(std::chrono::milliseconds tickMs)
    {
        using namespace std::chrono_literals;
        if (0ms == tickMs)  return;
        TickEvent::instance().setTimer(tickMs.count());
        this->mMainEventQueue_.submitTimerTick();
    }

    void TcpServer::setReadCallback(IoEventCallback cb) noexcept { this->mPool_->setReadCallback(cb); }
    void TcpServer::setWriteCallback(IoEventCallback cb) noexcept { this->mPool_->setWriteCallback(cb); }
    void TcpServer::setErrorCallback(ErrorCallback cb) noexcept { this->mPool_->setErrorCallback(cb); }

    void TcpServer::setSignalCallback(int sig, SignalCallback cb) noexcept
    {
        this->mSignalCbs_[sig] = cb; 
        this->mMainEventQueue_.submitSysSignal(sig);
    }

    void TcpServer::setTimeoutCallback(TimeoutCallback cb, std::chrono::milliseconds timeoutMs) noexcept
    {
        this->mTimer_.registTimeoutCallback(cb, timeoutMs);
    }
}   // namespace blitz
