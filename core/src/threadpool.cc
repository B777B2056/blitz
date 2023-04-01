#include "threadpool.h"

namespace blitz
{
    ThreadSafeQueue::ThreadSafeQueue(ThreadSafeQueue&& rhs)
        : mt{}, cv{}
    {
        std::unique_lock lock{rhs.mt};
        this->q = std::move(rhs.q);
    }

    ThreadSafeQueue& ThreadSafeQueue::operator=(ThreadSafeQueue&& rhs)
    {
        if (this != &rhs)
        {
            std::unique_lock lock{this->mt, std::defer_lock};
            std::unique_lock rhslock{rhs.mt, std::defer_lock};
            std::lock(lock, rhslock);
            this->q = std::move(rhs.q);
        }
        return *this;
    }

    void ThreadSafeQueue::enqueue(std::coroutine_handle<> coro) noexcept
    {
        std::unique_lock lock{this->mt};
        this->q.push(coro);
        this->cv.notify_one();
    }

    std::optional<std::coroutine_handle<>> ThreadSafeQueue::dequeue() noexcept
    {
        std::unique_lock lock{this->mt};
        if (this->q.empty())
        {
            return {};
        }
        auto coro = this->q.front();
        this->q.pop();
        return coro;
    }

    ThreadPool::ThreadPool(unsigned int threadNum)
        : mThreadNum_{threadNum}
    {
        this->mThreads_ = new std::jthread[threadNum];
        for (unsigned int i = 0; i < this->mThreadNum_; ++i)
        {
            this->mThreads_[i] = std::move(std::jthread
            {
                [this](std::stop_token stoken)->void
                {
                    while (!stoken.stop_requested())
                    {
                        auto coro = this->mTaskQueue_.dequeue();
                        if (coro.has_value())
                        {
                            coro.value().resume();
                        }
                    }
                }
            });
        }
    }

    ThreadPool::ThreadPool(ThreadPool&& rhs)
        : mThreads_{nullptr}
        , mThreadNum_{0}
    {
        *this = std::move(rhs);
    }

    ThreadPool& ThreadPool::operator=(ThreadPool&& rhs)
    {
        if (this != &rhs)
        {
            this->mThreads_ = rhs.mThreads_;
            this->mThreadNum_ = rhs.mThreadNum_;
            this->mTaskQueue_ = std::move(rhs.mTaskQueue_);
            rhs.mThreads_ = nullptr;
            rhs.mThreadNum_ = 0;
        }
        return *this;
    }

    ThreadPool::~ThreadPool()
    {
        for (unsigned int i = 0; i < this->mThreadNum_; ++i)
        {
            this->mThreads_[i].request_stop();
            this->mThreads_[i].join();
        }
        delete[] this->mThreads_;
    }

    void ThreadPool::submitTask(std::coroutine_handle<> coro) noexcept
    {
        this->mTaskQueue_.enqueue(coro);
    }
}   // namespace blitz
