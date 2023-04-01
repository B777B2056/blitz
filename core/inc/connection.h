#pragma once
#include <coroutine>
#include <span>
#include "buffer.h"
#include "common.h"
#include "ec.h"

namespace blitz
{
    class Connection : public Event
    {
    public:
        Connection(SocketDescriptor socket);
        ~Connection() = default;

        std::size_t read(std::span<char> buf, std::error_code& err);
        std::size_t write(std::span<const char> buf, std::error_code& err);

        ChainBuffer& readBuffer() { return this->mInputBuf_; }
        ChainBuffer& writeBuffer() { return this->mOutputBuf_; }

    private:
        ChainBuffer mInputBuf_;
        ChainBuffer mOutputBuf_;
    };
}   // namespace blitz
