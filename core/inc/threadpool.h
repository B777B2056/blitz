#pragma once
#include <coroutine>
#include <queue>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <future>

namespace blitz
{
    class ThreadPool
    {
    public:
        ThreadPool() = delete;
        ThreadPool(std::size_t threadNum);
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = default;
        ThreadPool& operator=(ThreadPool&&) = default;
        ~ThreadPool();

        std::size_t threadNum() const { return this->mThreads_.size(); }
        std::future<void> submitTask(std::function<void()> task);

    private:
        std::vector<std::jthread> mThreads_;
        std::mutex mMutex_;
        std::condition_variable mCv_;
        std::queue<std::function<void()>> mTaskQueue_;
    };

    class MultiThreadTaskAwaiter
    {
    public:
        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) noexcept
        {   
            auto future = this->mThreadPool_->submitTask([this]()->void
            {
                this->mTask_();
            });
            future.wait();
            handle.resume();
        }

        void await_resume() const noexcept {}

        MultiThreadTaskAwaiter(ThreadPool* t, std::function<void()> task) : mThreadPool_{t}, mTask_{task} {} 
    
    private:
        ThreadPool* mThreadPool_;
        std::function<void()> mTask_;
    };
}   // namespace blitz
