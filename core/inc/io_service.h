#pragma once
#include <coroutine>
#include <functional>
#include <unordered_map>
#include "acceptor.h"
#include "event_queue.h"
#include "threadpool.h"

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
        bool await_resume() const noexcept;

        IoTaskAwaiter(EventQueue* q, Connection* channel);

    private:
        bool isErr_;
        Connection* mConn_;
        EventQueue* mEventQueue_;
    };

    using IoEventCallback = std::function<void(Connection* conn)>;

    class IoService
    {
    public:
        IoService() = delete;
        IoService(Acceptor& acceptor, std::size_t threadNum);
        IoService(const IoService&) = delete;
        IoService& operator=(const IoService&) = delete;
        IoService(IoService&&) = default;
        IoService& operator=(IoService&&) = default;
        ~IoService();

        void registReadCallback(IoEventCallback cb) noexcept { this->mReadCb_ = cb; }
        void registWriteCallback(IoEventCallback cb) noexcept { this->mWriteCb_ = cb; }
        void registErrorCallback(IoEventCallback cb) noexcept { this->mErrCb_ = cb; }
        void registTimeoutCallback(IoEventCallback cb) noexcept { this->mTimeoutCb_ = cb; }

        void run();
        void closeConn(Connection* conn);

    private:
        Acceptor& mAcceptor_;
        EventQueue mEventQueue_;
        IoEventCallback mReadCb_, mWriteCb_, mErrCb_, mTimeoutCb_;
        ThreadPool mThreadPool_;
        std::unordered_map<Connection*, AsyncTask> mConns_;

        AsyncTask asyncHandle(Connection* conn);
    };
}   // namespace blitz
