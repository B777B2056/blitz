#include "threadpool.h"
#include "io_service.h"

namespace blitz
{
    IoServicePool::IoServicePool(std::size_t threadNum)
        : mNextIoServiceIdx_{0}, mIoServices_{threadNum}
    {

    }

    IoServicePool::~IoServicePool()
    {
        for (auto& t : this->mThreads_)
        {
            t.request_stop();
        }
        for (auto& service : this->mIoServices_)
        {
            service.wakeupFromWait();
        }
    }

    void IoServicePool::start(Timer& t)
    {
        for (std::size_t i = 0; i < this->mIoServices_.size(); ++i)
        {
            this->mThreads_.emplace_back([this, i, &t](std::stop_token stoken)->void
            {
                while (!stoken.stop_requested())
                {
                    this->mIoServices_[i].runOnce(t);
                }
            });
        }
    }

    void IoServicePool::putNewConnection(Connection* conn)
    {
        auto& service = this->nextIoService();
        service.registConnection(conn);
    }

    void IoServicePool::setReadCallback(IoEventCallback cb) noexcept
    {
        for (auto& service : this->mIoServices_)
        {
            service.setReadCallback(cb);
        }
    }

    void IoServicePool::setWriteCallback(IoEventCallback cb) noexcept
    {
        for (auto& service : this->mIoServices_)
        {
            service.setWriteCallback(cb);
        }
    }

    void IoServicePool::setErrorCallback(ErrorCallback cb) noexcept
    {
        for (auto& service : this->mIoServices_)
        {
            service.setErrorCallback(cb);
        }
    }

    IoService& IoServicePool::nextIoService()
    {
        auto& service = this->mIoServices_[this->mNextIoServiceIdx_ % this->mIoServices_.size()];
        ++this->mNextIoServiceIdx_;
        return service;
    }
}   // namespace blitz
