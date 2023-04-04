#include "io_service.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include "connection.h"

#define ASYNC_EXEC(THREAD_POOL, CONN, CB)   \
    if (CB)  \
    {   \
        if (0 == THREAD_POOL.threadNum())    \
        {   \
            CB(CONN);    \
        }   \
        else    \
        {   \
            co_await MultiThreadTaskAwaiter{&THREAD_POOL, [CONN, this]()->void   \
            {   \
                CB(CONN);    \
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
        : isErr_{false}, mConn_{conn}, mEventQueue_{q}
    {

    }

    bool IoTaskAwaiter::await_ready() const noexcept
    { 
        return false; 
    }

    void IoTaskAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept 
    {
        if (!this->mConn_)  return;
        this->isErr_ = (this->mEventQueue_->submitIoEvent(this->mConn_) < 0);
    }

    bool IoTaskAwaiter::await_resume() const noexcept
    { 
        return this->isErr_; 
    }

    IoService::IoService(Acceptor& acceptor, std::size_t threadNum)
        : mAcceptor_{acceptor}, mThreadPool_{threadNum}
    {
        this->mEventQueue_.submitAccept(this->mAcceptor_);
    }

    IoService::~IoService()
    {

    }

    void IoService::closeConn(Connection* conn)
    {
        if (!conn)  return;
        conn->setEvent(EventType::CLOSED);
        this->mEventQueue_.submitCloseConn(conn);
    }

    void IoService::run()
    {
        for (;;)
        {
            auto* conn = static_cast<Connection*>(this->mEventQueue_.waitCompletionEvent());
            if (!conn)  continue;
            if (conn->isClosed())
            {
                this->mConns_.erase(conn);
                delete conn;
            }
            else if (conn->isAccept())
            {
                this->mConns_[conn] = this->asyncHandle(conn);
                this->mEventQueue_.submitAccept(this->mAcceptor_);
            }
            else if (conn->isRead() || conn->isWrite())
            {
                // 恢复IO协程
                this->mConns_[conn].resume();
            }
            else if (conn->isTimeout())
            {
                [this, conn]()->AsyncTask
                {
                    ASYNC_EXEC(this->mThreadPool_, conn, this->mTimeoutCb_);
                }();
            }
        }
    }

    AsyncTask IoService::asyncHandle(Connection* conn)
    {
        if (!conn)  co_return;
        // 读入缓冲区
        conn->setEvent(EventType::READ);
        if (bool err = co_await IoTaskAwaiter{&this->mEventQueue_, conn}; err)
        {
            // 读入出错，执行错误回调
            ASYNC_EXEC(this->mThreadPool_, conn, this->mErrCb_);
            co_return;
        }
        // 在线程池中执行用户业务逻辑
        ASYNC_EXEC(this->mThreadPool_, conn, this->mReadCb_);
        // 写入缓冲区
        conn->setEvent(EventType::WRITE);
        if (bool err = co_await IoTaskAwaiter{&this->mEventQueue_, conn}; err)
        {
            // 写入出错，执行错误回调
            ASYNC_EXEC(this->mThreadPool_, conn, this->mErrCb_);
            co_return;
        }
        // 在线程池中执行用户业务逻辑
        ASYNC_EXEC(this->mThreadPool_, conn, this->mWriteCb_);
        co_return;
    }
}   // namespace blitz
