#include "connection.h"

namespace blitz
{
    Connection::Connection(SocketDescriptor socket)
        : Event{socket}
    {

    }

    std::size_t Connection::read(std::span<char> buf, std::error_code& err)
    {
        std::size_t n = this->mInputBuf_.readFromBuffer(buf);
        err = (0 == n) ? ErrorCode::PeerClosed : ErrorCode::Success;
        return n;
    }

    std::size_t Connection::write(std::span<const char> buf, std::error_code& err)
    {
        std::size_t n = this->mOutputBuf_.writeIntoBuffer(buf);
        err = (0 == n) ? ErrorCode::InternalError : ErrorCode::Success;
        return n;
    }
}   // namespace blitz
