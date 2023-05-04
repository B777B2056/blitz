#pragma once
#include <thread>
#include <vector>
#include "common.h"

namespace blitz
{
    class Connection;
    class IoService;
    class Timer;

    class IoServicePool
    {
    public:
        IoServicePool(std::size_t threadNum);
        ~IoServicePool();
        
        void start(Timer& t);
        void putNewConnection(Connection* conn);

        void setReadCallback(IoEventCallback cb) noexcept;
        void setWriteCallback(IoEventCallback cb) noexcept;
        void setErrorCallback(ErrorCallback cb) noexcept;

    private:
        std::size_t mNextIoServiceIdx_;
        std::vector<IoService> mIoServices_;
        std::vector<std::jthread> mThreads_;

        IoService& nextIoService();
    };
}   // namespace blitz
