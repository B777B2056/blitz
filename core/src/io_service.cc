#include "io_service.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include "connection.h"

namespace blitz
{
    auto IoTask::promise_type::get_return_object()
    {
        return IoTask{std::coroutine_handle<IoTask::promise_type>::from_promise(*this)};
    }

    void IoTask::promise_type::unhandled_exception() noexcept
    {

    }

    auto IoTask::promise_type::initial_suspend() noexcept 
    { 
        return std::suspend_never{}; 
    }

    auto IoTask::promise_type::final_suspend() noexcept 
    { 
        return std::suspend_always{}; 
    }

    IoTask::IoTask(std::coroutine_handle<> hdl)
        : coroutineHandle{hdl}
    {

    }

    IoTask::IoTask(IoTask&& rhs)
        : coroutineHandle{rhs.coroutineHandle}
    {
        rhs.coroutineHandle = {};
    }

    IoTask& IoTask::operator=(IoTask&& rhs)
    {
        if (this != &rhs)
        {
            if (this->coroutineHandle)
            {
                this->coroutineHandle.destroy();
            }
            this->coroutineHandle = rhs.coroutineHandle;
            rhs.coroutineHandle = {};
        }
        return *this;
    }

    void IoTask::resume()
    {
        if (this->coroutineHandle)
        {
            this->coroutineHandle.resume();
        }
    }

    IoAwaiter::IoAwaiter(EventQueue* q, Connection* conn) 
        : isErr_{false}, mConn_{conn}, mEventQueue_{q}
    {

    }

    bool IoAwaiter::await_ready() const noexcept
    { 
        return false; 
    }

    void IoAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept 
    {
        this->isErr_ = (this->mEventQueue_->submitIoEvent(this->mConn_) < 0);
    }

    bool IoAwaiter::await_resume() const noexcept
    { 
        return this->isErr_; 
    }

    IoService::IoService(Acceptor& acceptor)
        : mAcceptor_{acceptor}
    {
        this->mEventQueue_.submitAccept(this->mAcceptor_);
    }

    IoService::~IoService()
    {

    }

    void IoService::closeConn(Connection* conn)
    {
        this->mEventQueue_.closeConn(conn);
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
                this->mConns_.emplace(conn, this->asyncHandle(conn));
                this->mEventQueue_.submitAccept(this->mAcceptor_);
            }
            else if (conn->isRead())
            {
                // 恢复协程
                this->mConns_[conn].resume();
            }
            else if (conn->isWrite())
            {
                // 恢复协程
                this->mConns_[conn].resume();
            }
            else if (conn->isTimeout())
            {

            }
        }
    }

    IoTask IoService::asyncHandle(Connection* conn)
    {
        if (!conn)  co_return;
        // 读入缓冲区
        conn->setEvent(EventType::READ);
        bool err = co_await IoAwaiter{&this->mEventQueue_, conn};
        if (err)
        {
            // 读入出错，执行错误回调
            if (this->mErrCb_)  this->mErrCb_(conn);
            std::cerr << "co_await READ: " << ::strerror(errno) << std::endl;
            co_return;
        }
        // 在线程池中执行用户业务逻辑回调
        if (this->mReadCb_) this->mReadCb_(conn);
        // 写入缓冲区
        conn->setEvent(EventType::WRITE);
        err = co_await IoAwaiter{&this->mEventQueue_, conn};
        if (err)
        {
            // 写入出错，执行错误回调
            if (this->mErrCb_)  this->mErrCb_(conn);
            std::cerr << "co_await WRITE: " << ::strerror(errno) << std::endl;
            co_return;
        }
        // 在线程池中执行用户业务逻辑回调
        if (this->mWriteCb_)    this->mWriteCb_(conn);
        co_return;
    }
}   // namespace blitz
