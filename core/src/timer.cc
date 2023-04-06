#include "timer.h"

namespace blitz
{
    void Timer::tick() noexcept
    {
        while (!this->mTimeHeap_.empty())
        {
            auto& info = *this->mTimeHeap_.begin();
            if (!info.expired()) break;
            if (auto* conn = info.connection(); conn)
            {
                if (this->mCb_) this->mCb_(conn);
            }
            this->mTimeHeap_.erase(this->mTimeHeap_.begin());
        }
    }

    void Timer::registTimeoutCallback(TimeoutCallback cb, std::chrono::milliseconds timeoutMs) noexcept 
    { 
        this->mCb_ = cb; 
        this->mTimeoutMs_ = timeoutMs; 
    }

    void Timer::add(Connection* conn) noexcept
    {
        using namespace std::chrono_literals;
        if (this->mTimeoutMs_ == 0ms)   return;
        this->mTimeHeap_.emplace(TimerInfo{conn, this->mTimeoutMs_});
    }

    void Timer::remove(Connection* conn) noexcept
    {
        for (auto it = this->mTimeHeap_.begin(); it != this->mTimeHeap_.end();)
        {
            if (it->connection() == conn)
            {
                it = this->mTimeHeap_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}   // namespace blitz
