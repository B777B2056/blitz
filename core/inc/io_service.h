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
    struct IoTask
    {
        struct promise_type
        {
            auto get_return_object();
            void unhandled_exception() noexcept;
            void return_void() noexcept {} 
            auto initial_suspend() noexcept;
            auto final_suspend() noexcept;
        };

        std::coroutine_handle<> coroutineHandle;

        IoTask() = default;
        explicit IoTask(std::coroutine_handle<> hdl);
        IoTask(const IoTask&) = delete;
        IoTask& operator=(const IoTask&) = delete;
        IoTask(IoTask&& rhs);
        IoTask& operator=(IoTask&& rhs);
        ~IoTask() = default;

        void resume();
    };

    class IoAwaiter
    {
    public:
        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        bool await_resume() const noexcept;

        IoAwaiter(EventQueue* q, Connection* channel);

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
        IoService(std::uint16_t port, unsigned int threadNum, int backlog = 5);
        IoService(const IoService&) = delete;
        IoService& operator=(const IoService&) = delete;
        IoService(IoService&& rhs);
        IoService& operator=(IoService&& rhs);
        ~IoService();

        void registReadCallback(IoEventCallback cb) noexcept { this->mReadCb_ = cb; }
        void registWriteCallback(IoEventCallback cb) noexcept { this->mWriteCb_ = cb; }
        void registErrorCallback(IoEventCallback cb) noexcept { this->mErrCb_ = cb; }

        void run();
        void closeConn(Connection* conn);

    private:
        Acceptor mAcceptor_;
        EventQueue mEventQueue_;
        ThreadPool mThreadPool_;
        IoEventCallback mReadCb_, mWriteCb_, mErrCb_;

        std::unordered_map<Connection*, IoTask> mConns_;

        IoTask asyncHandle(Connection* conn);
    };
}   // namespace blitz
