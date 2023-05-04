#include "io_service.h"
#include <cerrno>
#include <cstring>
#include "connection.h"
#include "timer.h"

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

    IoService::~IoService()
    {
        for (auto& [conn, _] : this->mConns_)
        {
            this->closeConnection(conn);
        }
    }

    void IoService::registConnection(Connection* conn)
    {
        this->mConns_[conn] = this->asyncHandle(conn);
    }

    void IoService::wakeupFromWait()
    {
        // 注册一个任意事件，唤醒io_uring
        this->mEventQueue_.submitSysSignal(SIGINT);
    }

    void IoService::runOnce(Timer& t)
    {
        std::error_code ec;
        auto* conn = static_cast<Connection*>(this->mEventQueue_.waitCompletionEvent(ec));
        if (!conn)  return;
        if (ec != ErrorCode::Success)
        {
            this->mErrCb_(conn, ec);
            return;
        }
        if (conn->isClosing())
        {
            this->closeConnection(conn);
        }
        else if (conn->isClosed())
        {
            t.remove(conn);
            this->mConns_.erase(conn);
            delete conn;
        }
        else if (conn->isRead() || conn->isWrite())
        {
            // 恢复IO协程
            this->mConns_[conn].resume();
        }
    }

    void IoService::closeConnection(Connection* conn)
    {
        if (!conn)  return;
        conn->setEvent(EventType::CLOSED);
        this->mEventQueue_.submitCloseConn(conn);
    }

    AsyncTask IoService::asyncHandle(Connection* conn)
    {
        if (!conn)  co_return;
        // 读入缓冲区
        conn->setEvent(EventType::READ);
        if (auto ec = co_await IoTaskAwaiter{&this->mEventQueue_, conn}; ec != ErrorCode::Success)
        {
            // 读入出错，执行错误回调
            this->mErrCb_(conn, ec);
            co_return;
        }
        // 在线程池中执行用户业务逻辑
        this->mReadCb_(conn);
        // 写入缓冲区
        if (!conn)  co_return;
        conn->setEvent(EventType::WRITE);
        if (auto ec = co_await IoTaskAwaiter{&this->mEventQueue_, conn}; ec != ErrorCode::Success)
        {
            // 写入出错，执行错误回调
            this->mErrCb_(conn, ec);
            co_return;
        }
        // 在线程池中执行用户业务逻辑
        this->mWriteCb_(conn);
        co_return;
    }
}   // namespace blitz
