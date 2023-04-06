#include "io_service.h"
#include <cerrno>
#include <cstring>
#include "connection.h"

#define ASYNC_EXEC(THREAD_POOL, CB, ARGS...)   \
    if (CB)  \
    {   \
        if (0 == THREAD_POOL.threadNum())    \
        {   \
            CB(ARGS);    \
        }   \
        else    \
        {   \
            co_await MultiThreadTaskAwaiter{&THREAD_POOL, [ARGS, this]()->void   \
            {   \
                CB(ARGS);    \
            }}; \
        }   \
    }   \

namespace blitz
{
    auto AsyncTask::promise_type::get_return_object()
    {
        return AsyncTask{std::coroutine_handle<AsyncTask::promise_type>::from_promise(*this)};
    }

    void AsyncTask::promise_type::unhandled_exception() noexcept
    {

    }

    auto AsyncTask::promise_type::initial_suspend() noexcept 
    { 
        return std::suspend_never{}; 
    }

    auto AsyncTask::promise_type::final_suspend() noexcept 
    { 
        return std::suspend_always{}; 
    }

    AsyncTask::AsyncTask(std::coroutine_handle<> hdl)
        : mCoroutineHandle_{hdl}
    {

    }

    AsyncTask::AsyncTask(AsyncTask&& rhs)
        : mCoroutineHandle_{rhs.mCoroutineHandle_}
    {
        rhs.mCoroutineHandle_ = {};
    }

    AsyncTask& AsyncTask::operator=(AsyncTask&& rhs)
    {
        if (this != &rhs)
        {
            if (this->mCoroutineHandle_)
            {
                this->mCoroutineHandle_.destroy();
            }
            this->mCoroutineHandle_ = rhs.mCoroutineHandle_;
            rhs.mCoroutineHandle_ = {};
        }
        return *this;
    }

    AsyncTask::~AsyncTask()
    {
        if (this->mCoroutineHandle_)
        {
            this->mCoroutineHandle_.destroy();
        }
    }

    void AsyncTask::resume()
    {
        if (this->mCoroutineHandle_)
        {
            this->mCoroutineHandle_.resume();
        }
    }

    IoTaskAwaiter::IoTaskAwaiter(EventQueue* q, Connection* conn) 
        : ec{make_error_code(ErrorCode::Success)}, mConn_{conn}, mEventQueue_{q}
    {

    }

    bool IoTaskAwaiter::await_ready() const noexcept
    { 
        return false; 
    }

    void IoTaskAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept 
    {
        if (!this->mConn_)  return;
        this->ec = this->mEventQueue_->submitIoEvent(this->mConn_);
    }

    std::error_code IoTaskAwaiter::await_resume() const noexcept
    { 
        return this->ec; 
    }

    IoService::IoService(Acceptor& acceptor, std::size_t threadNum, std::chrono::milliseconds tickMs)
        : mIsLoopStop_{false}, mAcceptor_{acceptor}, mThreadPool_{threadNum}, mTickMs_{tickMs}
    {
        this->mEventQueue_.submitAccept(this->mAcceptor_);
        using namespace std::chrono_literals;
        if (this->mTickMs_ != 0ms)
        {
            TickEvent::instance().setTimer(this->mTickMs_.count());
            this->mEventQueue_.submitTimerTick();
        }
    }

    IoService::~IoService()
    {

    }

    void IoService::closeConn(Connection* conn)
    {
        if (!conn)  return;
        conn->setEvent(EventType::CLOSED);
        this->mEventQueue_.submitCloseConn(conn);
        this->mTimer_.remove(conn);
    }

    void IoService::registTimeoutCallback(TimeoutCallback cb, std::chrono::milliseconds timeoutMs) noexcept
    {
        this->mTimer_.registTimeoutCallback(cb, timeoutMs);
    }

    void IoService::registSignalCallback(int sig, SignalCallback cb) noexcept 
    { 
        this->mSignalCbs_[sig] = cb; 
        this->mEventQueue_.submitSysSignal(sig);
    }

    void IoService::run()
    {
        std::error_code ec;
        while (!this->mIsLoopStop_)
        {
            auto* event = this->mEventQueue_.waitCompletionEvent(ec);
            if (!event)  continue;
            if (event->isSignal())
            {
                auto* sigEv = static_cast<SignalEvent*>(event);
                auto cb = this->mSignalCbs_[sigEv->curSignal()];
                if (cb) cb();
                continue;
            }
            else if (event->isTick())
            {
                this->mTimer_.tick();
                TickEvent::instance().setTimer(this->mTickMs_.count());
                this->mEventQueue_.submitTimerTick();
                continue;
            }
            auto* conn = static_cast<Connection*>(event);
            if (ec != ErrorCode::Success)
            {
                [this, conn, ec]()->AsyncTask
                {   
                    ASYNC_EXEC(this->mThreadPool_, this->mErrCb_, conn, ec);
                }();
                continue;
            }
            if (conn->isClosed())
            {
                this->mConns_.erase(conn);
                delete conn;
            }
            else if (conn->isAccept())
            {
                this->mTimer_.add(conn);
                this->mConns_[conn] = this->asyncHandle(conn);
                this->mEventQueue_.submitAccept(this->mAcceptor_);
            }
            else if (conn->isRead() || conn->isWrite())
            {
                // 恢复IO协程
                this->mConns_[conn].resume();
            }
        }
    }

    AsyncTask IoService::asyncHandle(Connection* conn)
    {
        if (!conn)  co_return;
        // 读入缓冲区
        conn->setEvent(EventType::READ);
        if (auto ec = co_await IoTaskAwaiter{&this->mEventQueue_, conn}; ec != ErrorCode::Success)
        {
            // 读入出错，执行错误回调
            ASYNC_EXEC(this->mThreadPool_, this->mErrCb_, conn, ec);
            co_return;
        }
        // 在线程池中执行用户业务逻辑
        ASYNC_EXEC(this->mThreadPool_, this->mReadCb_, conn);
        // 写入缓冲区
        if (!conn)  co_return;
        conn->setEvent(EventType::WRITE);
        if (auto ec = co_await IoTaskAwaiter{&this->mEventQueue_, conn}; ec != ErrorCode::Success)
        {
            // 写入出错，执行错误回调
            ASYNC_EXEC(this->mThreadPool_, this->mErrCb_, conn, ec);
            co_return;
        }
        // 在线程池中执行用户业务逻辑
        ASYNC_EXEC(this->mThreadPool_, this->mWriteCb_, conn);
        co_return;
    }
}   // namespace blitz
