#pragma once
#include <coroutine>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace blitz
{
    class ThreadSafeQueue
    {
    public:
        ThreadSafeQueue() = default;
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue(ThreadSafeQueue&& rhs);
        ThreadSafeQueue& operator=(ThreadSafeQueue&& rhs);
        ~ThreadSafeQueue() = default;
        void enqueue(std::coroutine_handle<> coro) noexcept; 
        std::coroutine_handle<> dequeue() noexcept;

    private:
        mutable std::mutex mt;
        std::condition_variable cv;
        std::queue<std::coroutine_handle<>> q;
    };

    class ThreadPool
    {
    public:
        ThreadPool(unsigned int threadNum);
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&& rhs);
        ThreadPool& operator=(ThreadPool&& rhs);
        ~ThreadPool();

        auto schedule()
        {
            struct ThreadPoolAwaiter : std::suspend_always
            {
                ThreadPool* threadpool;
                void await_suspend(std::coroutine_handle<> coro) noexcept 
                {
                    threadpool->submitTask(coro);
                }
            };
            return ThreadPoolAwaiter{.threadpool = this};
        }
        
    private:
        std::jthread* mThreads_;
        unsigned int mThreadNum_;
        ThreadSafeQueue mTaskQueue_;

        void submitTask(std::coroutine_handle<> coro) noexcept;
    };
}   // namespace blitz
