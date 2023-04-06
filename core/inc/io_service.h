#pragma once
#include <coroutine>
#include <functional>
#include <unordered_map>
#include "acceptor.h"
#include "ec.h"
#include "event_queue.h"
#include "threadpool.h"
#include "timer.h"

#ifdef __linux__
    #define SIGNAL_NUM 32
#elif _WIN32
    #define SIGNAL_NUM 6
#endif

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

        IoTaskAwaiter(EventQueue* q, Connection* channel);

    private:
        std::error_code ec;
        Connection* mConn_;
        EventQueue* mEventQueue_;
    };

    using SignalCallback = std::function<void()>;
    using IoEventCallback = std::function<void(Connection* conn)>;
    using ErrorCallback = std::function<void(Connection* conn, std::error_code ec)>;

    class IoService
    {
    public:
        IoService() = delete;
        IoService(Acceptor& acceptor, std::size_t threadNum, std::chrono::milliseconds tickMs);
        IoService(const IoService&) = delete;
        IoService& operator=(const IoService&) = delete;
        IoService(IoService&&) = default;
        IoService& operator=(IoService&&) = default;
        ~IoService();

        void registReadCallback(IoEventCallback cb) noexcept { this->mReadCb_ = cb; }
        void registWriteCallback(IoEventCallback cb) noexcept { this->mWriteCb_ = cb; }
        void registErrorCallback(ErrorCallback cb) noexcept { this->mErrCb_ = cb; }
        void registTimeoutCallback(TimeoutCallback cb, std::chrono::milliseconds timeoutMs) noexcept;
        void registSignalCallback(int sig, SignalCallback cb) noexcept;

        void run();
        void stop() { this->mIsLoopStop_ = true; }
        void closeConn(Connection* conn);

    private:
        bool mIsLoopStop_{false};
        Acceptor& mAcceptor_;
        EventQueue mEventQueue_;
        Timer mTimer_;
        ErrorCallback mErrCb_;
        IoEventCallback mReadCb_, mWriteCb_;
        SignalCallback mSignalCbs_[SIGNAL_NUM];
        ThreadPool mThreadPool_;
        std::chrono::milliseconds mTickMs_;
        std::unordered_map<Connection*, AsyncTask> mConns_;

        AsyncTask asyncHandle(Connection* conn);
    };
}   // namespace blitz
