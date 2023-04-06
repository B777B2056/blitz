#pragma once
#include <chrono>
#include <functional>
#include <set>

namespace blitz
{
    class Connection;

    using TimeoutCallback = std::function<void(Connection* conn)>;

    class Timer
    {
    public:
        void tick() noexcept;
        void registTimeoutCallback(TimeoutCallback cb, std::chrono::milliseconds timeoutMs) noexcept;
        void add(Connection* conn) noexcept;
        void remove(Connection* conn) noexcept;

    private:
        class TimerInfo
        {
        public:
            TimerInfo(Connection* conn, std::chrono::milliseconds timeout)
             : conn_{conn}, time_{std::chrono::steady_clock::now() + timeout} {}

            bool expired() const noexcept { return std::chrono::steady_clock::now() >= time_; }
            Connection* connection() const noexcept { return this->conn_; }

            bool operator<(const TimerInfo& rhs) const { return time_ < rhs.time_; }
            bool operator<=(const TimerInfo& rhs) const { return time_ <= rhs.time_; }
            bool operator>(const TimerInfo& rhs) const { return time_ > rhs.time_; }
            bool operator>=(const TimerInfo& rhs) const { return time_ >= rhs.time_; }
            bool operator==(const TimerInfo& rhs) const { return conn_ == rhs.conn_; }
            bool operator!=(const TimerInfo& rhs) const { return conn_ != rhs.conn_; }

        private:
            mutable Connection* conn_;
            std::chrono::time_point<std::chrono::steady_clock> time_;
        };

    private:
        TimeoutCallback mCb_;
        std::chrono::milliseconds mTimeoutMs_;
        std::set<TimerInfo> mTimeHeap_;
    };
}   // namespace blitz
