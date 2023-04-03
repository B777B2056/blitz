#include "threadpool.h"
#include <memory>
#include <cassert>

namespace blitz
{
    ThreadPool::ThreadPool(std::size_t threadNum)
    {
        for (std::size_t i = 0; i < threadNum; ++i)
        {
            this->mThreads_.emplace_back([this](std::stop_token stoken)->void
            {
                while (!stoken.stop_requested())
                {
                    std::function<void()> task;
                    {
                        std::unique_lock lock{this->mMutex_};
                        while (this->mTaskQueue_.empty())
                        {
                            this->mCv_.wait(lock);
                        }
                        task = this->mTaskQueue_.front();
                        this->mTaskQueue_.pop();
                    }
                    task();
                }
            });
        }
    }

    ThreadPool::~ThreadPool()
    {
        if (0 == this->threadNum())   return;
        for (auto& t : this->mThreads_)
        {
            t.request_stop();
            t.join();
        }
        this->mCv_.notify_all();
    }

    std::future<void> ThreadPool::submitTask(std::function<void()> task)
    {
        assert(this->threadNum() > 0);
        auto pkgTask = std::make_shared<std::packaged_task<void()>>(task);
        auto future = pkgTask->get_future();
        {
            std::lock_guard lock{this->mMutex_};
            this->mTaskQueue_.push([pkgTask]()->void{ (*pkgTask)(); });
        }
        this->mCv_.notify_one();
        return future;
    }
}   // namespace blitz
