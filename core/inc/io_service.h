#pragma once
#include <coroutine>
#include <functional>
#include <unordered_map>
#include "ec.h"
#include "event_queue.h"

namespace blitz
{
    // 将任务（Channel）提交到SQE（提交队列）后挂起协程
    // CQE（完成队列）有完成事件返回时恢复协程
    class AsyncTask
    {
    public:
        struct promise_type
        {
            auto get_return_object();
            void unhandled_exception() noexcept;
            void return_void() noexcept {} 
            auto initial_suspend() noexcept;
            auto final_suspend() noexcept;
        };

        AsyncTask() = default;
        explicit AsyncTask(std::coroutine_handle<> hdl);
        AsyncTask(const AsyncTask&) = delete;
        AsyncTask& operator=(const AsyncTask&) = delete;
        AsyncTask(AsyncTask&& rhs);
        AsyncTask& operator=(AsyncTask&& rhs);
        ~AsyncTask();

        void resume();

    private:
        std::coroutine_handle<> mCoroutineHandle_;
    };

    class IoTaskAwaiter
    {
    public:
        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        std::error_code await_resume() const noexcept;

        IoTaskAwaiter(EventQueue* q, Connection* conn);

    private:
        std::error_code ec;
        Connection* mConn_;
        EventQueue* mEventQueue_;
    };

    class Timer;

    class IoService
    {
    public:
        IoService() = default;
        IoService(const IoService&) = delete;
        IoService& operator=(const IoService&) = delete;
        IoService(IoService&&) = default;
        IoService& operator=(IoService&&) = default;
        ~IoService();

        void setReadCallback(IoEventCallback cb) noexcept { this->mReadCb_ = cb; }
        void setWriteCallback(IoEventCallback cb) noexcept { this->mWriteCb_ = cb; }
        void setErrorCallback(ErrorCallback cb) noexcept { this->mErrCb_ = cb; }

        void runOnce(Timer& t);
        void registConnection(Connection* conn);
        void wakeupFromWait();

    private:
        EventQueue mEventQueue_;
        ErrorCallback mErrCb_;
        IoEventCallback mReadCb_, mWriteCb_;
        std::unordered_map<Connection*, AsyncTask> mConns_;
        
        void closeConnection(Connection* conn);
        AsyncTask asyncHandle(Connection* conn);
    };
}   // namespace blitz
